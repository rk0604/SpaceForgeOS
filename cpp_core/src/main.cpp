/** Compile command:
 *  (Windows MinGW)
 *    g++ -std=c++17 main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp Logger.cpp -I ../include -o simulation
 *
 *  (macOS/Linux, Clang/GCC — threads need -pthread)
 *    g++ -std=c++17 main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp Logger.cpp -I ../include -pthread -o simulation
 *
 *  (Optional Ion file present but NOT used in this deposition-only main)
 *    g++ -std=c++17 main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp Logger.cpp ion_implantation_model.cpp -I ../include -pthread -o simulation
 *
 * Run command:
 *    ./simulation
 *
 * If using Windows PowerShell with a local MinGW toolchain:
 *    $env:Path = "C:\Users\Risha\Downloads\winlibs-x86_64-posix-seh-gcc-15.1.0-mingw-w64msvcrt-12.0.0-r1\mingw64\bin;" + $env:Path
 *    setx PATH "C:\Users\Risha\Downloads\winlibs-x86_64-posix-seh-gcc-15.1.0-mingw-w64msvcrt-12.0.0-r1\mingw64\bin;$env:PATH"
 */

// Header files 
// #include "OrbitModel.hpp"
#include "PowerBus.hpp"           
#include "DepositionModule.hpp"
// #include "IonImplantationModule.hpp"
// #include "CrystalGrowthModule.hpp"
#include "Logger.hpp"
#include "Task.hpp"

// needed imports 
#include <iostream>
#include <fstream>
#include <vector>   // a vector is a dynamically sized array with O(1)
#include <sstream>
#include <cstdlib>  // For rand(), srand()
#include <ctime>    // for time()
#include <thread>   // for threads - each module is a unique thread 
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

const int SIM_DURATION = 1440;  // 24 hours in minutes
int DEFECT_COUNT = 0;

// Function to load tasks from file
std::vector<Task*> loadTasksFromFile(const std::string& filename) {
    std::ifstream infile(filename);
    std::vector<Task*> tasksVector;               // each Task represents ONE wafer/job, store by pointer
    std::string line;

    while (std::getline(infile, line)) {
        Task* task = new Task();
        task->id = line;                          // e.g. T_1, T_2 ...

        // ---------- default phase durations ----------
        task->phase[0].requiredTime = 60;   // Deposition
        task->phase[1].requiredTime = 20;   // Ion Implantation
        task->phase[2].requiredTime = 120;  // Crystal Growth

        // ---------- initialise status flags ----------
        for (int i = 0; i < 3; ++i) {
            task->phase[i].wasInterrupted = false;
            task->phase[i].defective      = false;
            task->phase[i].elapsedTime    = 0;
            task->phase[i].energyUsed     = 0;
        }

        task->phase[0].defectChance = 0.010;
        task->phase[1].defectChance = 0.001;
        task->phase[2].defectChance = 0.025;

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
void logTaskVector(const std::vector<Task*>& tasks, std::ostream& out = std::cout) {
    for (const Task* task : tasks) {
        out << "Task ID: " << task->id << "\n";
        for (int i = 0; i < 3; ++i) {
            out << "  Phase "       << i
                << " | Required: "  << task->phase[i].requiredTime
                << " | Elapsed: "   << task->phase[i].elapsedTime
                << " | EnergyUsed: "<< task->phase[i].energyUsed
                << " | Interrupted: "<< (task->phase[i].wasInterrupted ? "Yes" : "No")
                << " | DefChance: " << task->phase[i].defectChance
                << " | Defective: " << (task->phase[i].defective     ? "Yes" : "No")
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
     * LoggerInstance
     * phaseName - holds the current phase and used for logging purposes 
     */
    PowerModule Power(250000, 300, 0);  // 250 000 "W·min" (≈ 250 Wh); bus enforces 300 W/min draw cap

    // std::ofstream outputFile = openCSVLogFile("logV1.csv"); - open the log file
    Logger LoggerInstance("../../scheduler_dl/data/logV1.csv");                          

    DepositionModule DepositionModuleInstance;
    // IonImplantationModule IonImplantationModuleInstance;  // removed for deposition-only run

    // load & enqueue pointers to the tasks
    std::vector<Task*> tasks = loadTasksFromFile("../../scheduler_dl/tasks1.txt");
    for (Task* task : tasks) {
        DepositionModuleInstance.enqueue(task);
        // IonImplantationModuleInstance.enqueueIonImplantation(task); // not used in deposition-only
    }

    // concurrency tools 
    std::mutex depo_mutex, power_mutex;                  // mutexes for deposition and powerBus
    std::condition_variable depo_cv;                     // tells a thread to "Wake up" 
    std::atomic<bool> keep_running(true);
    std::atomic<int>  simMinute(0);                      // since simMinute is atomic it cannot be interrupted by other threads
    std::atomic<int>  orbitState(0);                     // 0 = sunlight, 1 = eclipse

    // --- NEW: minute tick counter to avoid spurious wakeups repeating work ---
    std::atomic<int> tick(0);                            // incremented by main each minute

    // operate in the background and wait for main thread to call notify_one() to "wake up"
    std::thread deposition_thread([&]() {
        int seen = 0;  // last processed tick value (thread-local)
        while (keep_running) {
            std::unique_lock<std::mutex> lock(depo_mutex);

            // Wait until either shutdown requested or a NEW tick is available.
            depo_cv.wait(lock, [&]() {
                return !keep_running || tick.load(std::memory_order_acquire) > seen;
            });

            if (!keep_running) break;

            // Advance our watermark so one notify → at most one update call.
            seen = tick.load(std::memory_order_relaxed);

            // Do one minute of work for the deposition module.
            DepositionModuleInstance.update(
                simMinute.load(std::memory_order_relaxed),
                Power,
                LoggerInstance,
                &power_mutex,
                &orbitState
            );
        }
    });

    // main while loop
    for (int t = 0; t < SIM_DURATION; t++) {
        simMinute.store(t, std::memory_order_relaxed);                   // publish the current simulated minute
        orbitState.store((t % 90 < 45) ? 0 : 1, std::memory_order_relaxed); // 0 = sunlight, 1 = eclipse

        Power.update(t, (orbitState.load(std::memory_order_relaxed) == 0 ? "sunlight" : "eclipse"));

        // Publish a NEW tick and wake the worker exactly once per minute.
        tick.fetch_add(1, std::memory_order_release);
        depo_cv.notify_one();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    keep_running.store(false, std::memory_order_release);
    depo_cv.notify_one();  // wake them up to let them exit once they see that the keep_running flag is set to false 

    deposition_thread.join();

    // tidy up dynamically allocated tasks
    for (Task* t : tasks) {
        delete t;
    }

    return 0;
}
