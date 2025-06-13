/** Compile command:
 * g++ main.cpp PowerModule.cpp OrbitModel.cpp deposition_model.cpp -I ../include -o simulation
 * Run command:
 * ./simulation
 */

#include <iostream>
#include <fstream>
#include <vector> // a vector is a dynamically sized array with O(1)
#include <sstream>
#include "OrbitModel.hpp"
#include "PowerModule.hpp"
#include "DepositionModule.hpp"
#include "IonImplantationModule.hpp"
#include "CrystalGrowthModule.hpp"
#include "Logger.hpp"
#include "Task.hpp"
#include <cstdlib> // For rand(), srand()
#include <ctime>    // for time()

const int SIM_DURATION = 1440;  // 24 hours in minutes
int DEFECT_COUNT = 0;

// Function to load tasks from file
std::vector<Task> loadTasksFromFile(const std::string& filename) {
    std::ifstream infile(filename);
    std::vector<Task> tasksVector; // initialize a vector of task objects - each representing ONE wafer/job
    std::string line;

    while (std::getline(infile, line)) { // opens the file for reading and reads line by line
        Task task; // initialize an empty task object
        task.id = line; // e.g. T_1 and so on...

        // Set default durations
        task.phase[0].requiredTime = 60;   // Deposition
        task.phase[1].requiredTime = 20;   // Ion Implantation
        task.phase[2].requiredTime = 120;  // Crystal Growth

        // all initialized to false 
        task.phase[0].wasInterrupted = false;
        task.phase[1].wasInterrupted = false;
        task.phase[2].wasInterrupted = false;

        task.phase[0].defectChance = 0.01;
        task.phase[1].defectChance = 0.001;
        task.phase[2].defectChance = 0.025;

        task.phase[0].defective = false;
        task.phase[1].defective = false;
        task.phase[2].defective = false;
        tasksVector.push_back(task); // back inside vector
    }

    return tasksVector;
}

// simply log to a csv file 
std::ofstream openCSVLogFile(const std::string& filename) {
    std::ofstream file(filename, std::ios::out);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        exit(1);  // or return std::ofstream() if you prefer not to exit
    }

    // Write headers
    file << "Minute|,Orbit|,Battery|,Available|,Task|,Phase|,Time_Done|\n";
    return file;
}

// log the task vector to terminal
void logTaskVector(const std::vector<Task>& tasks, std::ostream& out = std::cout) {
    for (const Task& task : tasks) {
        out << "Task ID: " << task.id << "\n";
        for (int i = 0; i < 3; ++i) {
            out << "  Phase " << i
                << " | Required: "     << task.phase[i].requiredTime
                << " | Elapsed: "      << task.phase[i].elapsedTime
                << " | Energy_used: "  << task.phase[i].energyUsed
                << " | Interrupted: "  << (task.phase[i].wasInterrupted ? "Yes" : "No")
                << " | DefectChance: " << task.phase[i].defectChance
                << " | Defective: "    << (task.phase[i].defective ? "Yes" : "No")
                << "\n";
        }
        out << "-------------------------\n";
    }
}

int main() {
    srand(static_cast<unsigned int>(time(nullptr))); // randomize defects per run
    // Initialize power module
    PowerModule Power(250000, 300, 0);  // 250 Wh battery, 300 W solar gen, 0W Eclipse
    // can only draw 300 watts per min from the battery at once

    // Load wafer tasks
    std::vector<Task> tasks = loadTasksFromFile("../../scheduler_dl/tasks1.txt");
    logTaskVector(tasks);

    int currentTaskIndex = 0;
    int t = 0;

    // open the output file to better view the logs
    std::ofstream outputFile = openCSVLogFile("power_module_testing3.csv");

    // Initialize the 3 phase modules OUTSIDE of the while loop
    DepositionModule DepositionModuleInstance;
    Logger logger; 

    // loop runs for N*200 iterations (E.g. 4 tasks ==> 800 iterations)
    while (t < SIM_DURATION && currentTaskIndex < tasks.size()) {
        Task& currentTask = tasks[currentTaskIndex];
        std::string orbitPhase = (t % 90 < 45) ? "sunlight" : "eclipse"; // orbit repeats every 90 mins with 45 minutes of sinlight

        //Check if the phase has an error, if so then go to next task
        std::string phaseName = "unknown";
        if (currentTask.currentStage == 0) phaseName = "deposition";
        else if (currentTask.currentStage == 1) phaseName = "ion_implantation";
        else if (currentTask.currentStage == 2) phaseName = "crystal_growth";

        if (currentTask.phaseFail()) {
            outputFile << t << "," << orbitPhase << "," << Power.getBatteryLevel() << "," << Power.getAvailablePower() << "," << currentTask.id
                    << "," << phaseName << "," << currentTask.currentPhase().elapsedTime << " ❌ DEFECT - Skipped" << std::endl;
            // Power budget for this minute
            Power.update(t, orbitPhase);        
            ++currentTaskIndex;
            ++t;
            DEFECT_COUNT++;
            continue;
        }


        DepositionModuleInstance.enqueue(currentTask);  // Pass by reference
        DepositionModule::runOneMinute(currentTask, Power, logger); //since it is a static method, call using class names

        // Power budget for this minute
        Power.update(t, orbitPhase);

        // Get the current phase info for the current task 
        Task::PhaseInfo& phase = currentTask.currentPhase();

        // set the current phase and required power for that phase
        int requiredPower = 0;
        // std::string phaseName;

        if (currentTask.currentStage == 0) {
            requiredPower = 300; 
            // ⚡ Deposition: 300 W power per minute × 60 min = 18,000 Wh (or 18 kWh per wafer)
            // Since power is in watts, each minute draws 300 W × (1/60) hr = 5 Wh = 5000 mWh from battery
            // 300 W is the power - to get Wh 
            // phaseName = "deposition";
        } else if (currentTask.currentStage == 1) {
            requiredPower = 200;
            // ⚡ Ion Implantation: 200 W per minute × 20 min = 4,000 Wh (4 kWh per wafer)
            // Each minute draws 200 W = 3.33 Wh = 3333 mWh
            // phaseName = "ion_implantation";
        } else if (currentTask.currentStage == 2) {
            requiredPower = 250;
            // ⚡ Crystal Growth: 250 W per minute × 120 min = 30,000 Wh (30 kWh per wafer)
            // Each minute draws 250 W = 4.17 Wh = 4167 mWh
            // phaseName = "crystal_growth";
        }

        bool progressed = false;
        if (Power.canSatisfyDemand(requiredPower)) {
            Power.consumePower(requiredPower);
            phase.elapsedTime += 1;
            phase.energyUsed += requiredPower;
            progressed = true; 
            // this phase of the task got sufficient power from the battery 
        }

        // Check if this phase is complete
        if (phase.isDone()) {
            currentTask.currentStage += 1;
        }

        // Print simulation status
        // std::cout << "Minute " << t
        //           << " | Orbit: " << orbitPhase
        //           << " | Battery: " << Power.getBatteryLevel()
        //           << " | Available: " << Power.getAvailablePower()
        //           << " | Task: " << currentTask.id
        //           << " | Phase: " << phaseName
        //           << " | Time Done: " << phase.elapsedTime
        //           << " / " << phase.requiredTime
        //           << (progressed ? " ✅ Progressed\n" : " ❌ Waiting\n");
        
        outputFile << t << "," << orbitPhase << "," << Power.getBatteryLevel() << "," << Power.getAvailablePower() << "," << currentTask.id
        << "," << phaseName << "," << phase.elapsedTime << (progressed ? " ✅ Progressed" : " ❌ Waiting") << std::endl;

        // If task is complete, move to next
        if (currentTask.isComplete()) {
            ++currentTaskIndex;
        }
        ++t;
    }

    std::cout << "Tasks skipped due to defects: " << DEFECT_COUNT << "\n";
    return 0;
}