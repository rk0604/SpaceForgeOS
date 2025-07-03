/** Compile command:
 * g++ main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp -I ../include -o simulation
 * g++ main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp Logger.cpp ion_implantation_model.cpp -I ../include -o simulation 
 * WITH threads: g++ -std=c++17 main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp ion_implantation_model.cpp Logger.cpp -I ../include -o simulation
 * Run command:
 * ./simulation
 * $env:Path = "C:\Users\Risha\Downloads\winlibs-x86_64-posix-seh-gcc-15.1.0-mingw-w64msvcrt-12.0.0-r1\mingw64\bin;" + $env:Path
 * setx PATH "C:\Users\Risha\Downloads\winlibs-x86_64-posix-seh-gcc-15.1.0-mingw-w64msvcrt-12.0.0-r1\mingw64\bin;$env:PATH"
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
std::vector<Task*> loadTasksFromFile(const std::string& filename) {
    std::ifstream infile(filename);
    std::vector<Task*> tasksVector;               // each Task represents ONE wafer/job, store by pointer
    std::string line;

    while (std::getline(infile, line)) {
        Task* task = new Task();
        task->id = line;                          // e.g. T_1, T_2 …

        // ---------- default phase durations ----------
        task -> phase[0].requiredTime = 60;   // Deposition
        task -> phase[1].requiredTime = 20;   // Ion Implantation
        task -> phase[2].requiredTime = 120;  // Crystal Growth

        // ---------- initialise status flags ----------
        for (int i = 0; i < 3; ++i) {
            task -> phase[i].wasInterrupted = false;
            task -> phase[i].defective      = false;
        }

        task -> phase[0].defectChance = 0.010;
        task -> phase[1].defectChance = 0.001;
        task -> phase[2].defectChance = 0.025;

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

    // std::ofstream outputFile = openCSVLogFile("logV1.csv"); 
    Logger LoggerInstance("../../scheduler_dl/data/logV1.csv");                          

    DepositionModule      DepositionModuleInstance;
    IonImplantationModule IonImplantationModuleInstance;

    // load & enqueue pointers to the tasks
    std::vector<Task*> tasks = loadTasksFromFile("../../scheduler_dl/tasks1.txt");
    for (Task* task : tasks) {
        DepositionModuleInstance.enqueue(task);
        IonImplantationModuleInstance.enqueueIonImplantation(task);
    }

    // concurrency tools 
    std::mutex depo_mutex, ion_mutex, crys_mutex, power_mutex;
    std::condition_variable depo_cv, ion_cv, crys_cv; // tells a thread to "Wake up" 
    std::atomic<bool> keep_running(true);
    std::atomic<int> simMinute(0); // since simMinute is atomic it cannot be interrupted by other threads
    std::atomic<int> orbitState(0);  // 0 = sunlight, 1 = eclipse

    // operate in the background and wait for main thread to call notify_one() to "wake up"
    std::thread deposition_thread([&]() {
        while (keep_running) {
            std::unique_lock<std::mutex> lock(depo_mutex);
            depo_cv.wait(lock); // 

            if (!keep_running) break;

            DepositionModuleInstance.update(simMinute.load(), Power, LoggerInstance, &power_mutex, &orbitState);
        }
    });

    // [&] captures everything by reference: DepositionModuleInstance, Power can be used directly inside the thread
    std::thread ion_thread([&]() {
        while (keep_running) {
            std::unique_lock<std::mutex> lock(ion_mutex);
            ion_cv.wait(lock);

            if (!keep_running) break;

            IonImplantationModuleInstance.update_imp(simMinute.load(), Power, LoggerInstance, &power_mutex, &orbitState);
        }
    });

    // main while loop
    for(int t = 0; t < SIM_DURATION; t++){
        simMinute.store(t); // used to atomatically replace the current value of std::atomic with a new value
        orbitState.store((t % 90 < 45) ? 0 : 1);  // 0 = sunlight, 1 = eclipse

        Power.update(t, (orbitState.load() == 0 ? "sunlight" : "eclipse"));

        depo_cv.notify_one();
        ion_cv.notify_one();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    keep_running = false;
    depo_cv.notify_one();  // wake them up to let them exit once they see that the keep_running flag is set to false 
    ion_cv.notify_one();

    deposition_thread.join();
    ion_thread.join();

    return 0;
}
