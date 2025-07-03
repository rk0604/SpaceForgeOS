/** Compile command:
 * g++ main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp -I ../include -o simulation
 * g++ main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp Logger.cpp ion_implantation_model.cpp -I ../include -o simulation 
 * Run command:
 * ./simulation
 */

 //Header files 
#include "OrbitModel.hpp"
#include "PowerModule.hpp"
#include "DepositionModule.hpp"
#include "IonImplantationModule.hpp"
#include "CrystalGrowthModule.hpp"
#include "Logger.hpp"
#include "Task.hpp"

// needed imports 
#include <iostream>
#include <fstream>
#include <vector> // a vector is a dynamically sized array with O(1)
#include <sstream>
#include <cstdlib> // For rand(), srand()
#include <ctime>   // for time()
#include <thread> // for threads - each module is a unique thread 
#include <mutex>
#include <condition_variable>
#include <atomic>

const int SIM_DURATION = 1440;  // 24 hours in minutes
int DEFECT_COUNT = 0;

// Function to load tasks from file
std::vector<Task> loadTasksFromFile(const std::string& filename) {
    std::ifstream infile(filename);
    std::vector<Task> tasksVector;               // each Task represents ONE wafer/job
    std::string line;

    while (std::getline(infile, line)) {
        Task task;
        task.id = line;                          // e.g. T_1, T_2 …

        // ---------- default phase durations ----------
        task.phase[0].requiredTime = 60;   // Deposition
        task.phase[1].requiredTime = 20;   // Ion Implantation
        task.phase[2].requiredTime = 120;  // Crystal Growth

        // ---------- initialise status flags ----------
        for (int i = 0; i < 3; ++i) {
            task.phase[i].wasInterrupted = false;
            task.phase[i].defective      = false;
        }

        task.phase[0].defectChance = 0.010;
        task.phase[1].defectChance = 0.001;
        task.phase[2].defectChance = 0.025;

        tasksVector.push_back(task);
    }
    return tasksVector;
}

// simply log to a csv file 
std::ofstream openCSVLogFile(const std::string& filename) {
    std::ofstream file(filename, std::ios::out);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        std::exit(1);
    }
    file << "Minute|,Orbit|,Battery(Wh)|,TotalPowerAvailable_preRUN|,Task|,Phase|,Time_Done|,Status|\n";
    return file;
}

// helper to dump internal task state to terminal (debug only)
void logTaskVector(const std::vector<Task>& tasks, std::ostream& out = std::cout) {
    for (const Task& task : tasks) {
        out << "Task ID: " << task.id << "\n";
        for (int i = 0; i < 3; ++i) {
            out << "  Phase "       << i
                << " | Required: "  << task.phase[i].requiredTime
                << " | Elapsed: "   << task.phase[i].elapsedTime
                << " | EnergyUsed: "<< task.phase[i].energyUsed
                << " | Interrupted: "<< (task.phase[i].wasInterrupted ? "Yes" : "No")
                << " | DefChance: " << task.phase[i].defectChance
                << " | Defective: " << (task.phase[i].defective     ? "Yes" : "No")
                << '\n';
        }
        out << "-------------------------\n";
    }
}

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));  // randomise defect RNG

    /**     INITIALISATIONS:
     * PowerModule - 250 Wh battery, 300 W solar gen, 0 W eclipse
     *             - can only draw 300 W per minute from the battery at once
     * Wafer Tasks into tasks vector
     * Task Index and current time to 0
     * openCSVLogFile - open the output file for logs
     * DepositionModuleInstance
     *  - for (Task& task : tasks) enqueue all tasks into the deposition module
     * IonImplantationModuleInstance
     * LoggerInstance
     * phaseName - holds the current phase and used for logging purposes 
     */
    PowerModule Power(250000, 300, 0);  // 250 000 mWh = 250 Wh
    int currentTaskIndex = 0;
    int current_t        = 0;

    // std::ofstream outputFile = openCSVLogFile("logV1.csv"); 
    Logger LoggerInstance("logV1.csv");                          

    DepositionModule      DepositionModuleInstance;
    IonImplantationModule IonImplantationModuleInstance;

    // load & enqueue pointers to the tasks
    std::vector<Task> tasks = loadTasksFromFile("../../scheduler_dl/tasks1.txt");
    for (Task& task : tasks) {
        DepositionModuleInstance.enqueue(&task);
        IonImplantationModuleInstance.enqueueIonImplantation(&task);
    }

    // concurrency tools 
    std::mutex depo_mutex, ion_mutex, crys_mutex;
    std::condition_variable depo_cv, ion_cv, crys_cv; // tells a thread to "Wake up" 
    std::atomic<bool> keep_running(true);
    std::atomic<int> simMinute(0);

    // operate in the background and wait for main thread to call notify_one() to "wake up"
    std::thread deposition_thread([&]() {
        while (keep_running) {
            std::unique_lock<std::mutex> lock(depo_mutex);
            depo_cv.wait(lock); // 

            if (!keep_running) break;

            DepositionModuleInstance.update(current_t, Power, LoggerInstance);
        }
    });

    //[&] captures everything by reference: DepositionModuleInstance, Power can be used directly inside the thread
    std::thread ion_thread([&]() {
        while (keep_running) {
            std::unique_lock<std::mutex> lock(ion_mutex);
            ion_cv.wait(lock);

            if (!keep_running) break;

            IonImplantationModuleInstance.update_imp(current_t, Power, LoggerInstance);
        }
    });




    // -------------------- MAIN SIMULATION LOOP ------------------------------
    while (current_t < SIM_DURATION && currentTaskIndex < static_cast<int>(tasks.size())) {
        
        Task& currentTask  = tasks[currentTaskIndex];
        std::string orbitPhase = (current_t % 90 < 45) ? "sunlight" : "eclipse";

        // map stage index -> readable phase name
        std::string phaseName = (currentTask.currentStage == 0) ? "deposition" :
                                (currentTask.currentStage == 1) ? "ion_implantation" :
                                (currentTask.currentStage == 2) ? "crystal_growth" : "unknown";

        // update available power for this minute
        Power.update(current_t, orbitPhase);

        // --------------------------------------------------------------------
        // Early-exit / failure detection  ( **only for implemented modules 0,1** )
        // --------------------------------------------------------------------
        if (currentTask.currentStage <= 1 &&    // guard: modules implemented
            (currentTask.phaseFail() || currentTask.phase[currentTask.currentStage].wasInterrupted)) {

            std::string phaseResult = "❌ UNKNOWN ERROR";
            if (currentTask.phase[currentTask.currentStage].wasInterrupted)    phaseResult = "❌ STALLED @" + phaseName;
            if (currentTask.phaseFail())    phaseResult = "❌ DEFECT - Skipped @" + phaseName;
            
            bool progressed = false;
            std::string logStatus = "✅ Progressed";

            if (currentTask.currentStage == 1) {  // ion implantation
                if (IonImplantationModuleInstance.isCalibrating())
                    logStatus = "⏸️ RECALIBRATING";
                else if (IonImplantationModuleInstance.isCoolingDown())
                    logStatus = "⏳ COOL DOWN";
                else if (!progressed)
                    logStatus = "❌ Waiting - Power";
            }

            //IMPORTANT!! LOOK AT MINUTE 58 IN LOG, TIMING OF ELAPASED TIME NOT CORRECT 

            LoggerInstance.log(
                current_t, orbitPhase,
                Power.getBatteryLevel() / 1000,
                Power.getAvailablePower(),
                currentTask.id, phaseName,
                currentTask.phase[currentTask.currentStage].elapsedTime,
                logStatus
            );

            ++currentTaskIndex;
            ++current_t;
            ++DEFECT_COUNT;
            continue;
        }

        // --------------------------------------------------------------------
        // Normal processing path
        // --------------------------------------------------------------------
        Task::PhaseInfo& phase = currentTask.currentPhase();

        int  requiredPower = 0;
        bool progressed    = false;

        if (currentTask.currentStage == 0) {
            requiredPower = 300;
                depo_cv.notify_one(); // call update via a dedicated thread 
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                deposition_thread.join();
            progressed = true;
        }
        else if (currentTask.currentStage == 1) {
            requiredPower = 200;
                ion_cv.notify_one(); // // call update via a dedicated thread 
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
                deposition_thread.join();
            progressed = true;
        }
        else if (currentTask.currentStage == 2) {
            requiredPower = 250;
            if (Power.canSatisfyDemand(requiredPower)) {
                Power.consumePower(requiredPower);
                phase.elapsedTime++;
                progressed = true;
            }
        }

        // did the phase finish?
        if (phase.isDone()) {
            currentTask.currentStage++;
        }

        // ---------------------- CSV outputs ---------------------------------
        LoggerInstance.log(
            current_t, orbitPhase,
            Power.getBatteryLevel() / 1000,
            Power.getAvailablePower(),
            currentTask.id, phaseName,
            phase.elapsedTime,
            progressed ? "✅ Progressed" : "❌ Waiting - Power"
        );

        // raw backup (kept from original snippet)
        // outputFile << current_t << "," << orbitPhase << ","
        //            << (Power.getBatteryLevel() / 1000) << ","
        //            << Power.getAvailablePower() << ","
        //            << currentTask.id << ","
        //            << phaseName << ","
        //            << phase.elapsedTime
        //            << (progressed ? " ✅ Progressed" : " ❌ Waiting")
        //            << '\n';

        // move to next task if completed
        if (currentTask.isComplete()) {
            ++currentTaskIndex;
        }
        ++current_t;
    }

    std::cout << "Tasks skipped due to defects: " << DEFECT_COUNT << '\n';

    keep_running = false;

    // wake up threads one last time so they can check keep_running and exit
    depo_cv.notify_one();
    ion_cv.notify_one();

    deposition_thread.join();
    ion_thread.join();
    return 0;
}
