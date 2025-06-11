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

// run with this command  g++ main.cpp PowerModule.cpp OrbitModel.cpp -I ../include -o simulation

const int SIM_DURATION = 1440;  // 24 hours in minutes

int main() {
    // Initialize power module
    PowerModule Power(10000, 300, 0);  // 1000mWh battery capacity, 300W sunlight, 0W eclipse
    // can only draw 300 watts per min from the battery at once

    // Load wafer tasks
    std::vector<Task> tasks = loadTasksFromFile("../../scheduler_dl/tasks1.txt");
    // print the tasks for viewing 
    for (const Task& task : tasks) {
        std::cout << "Task ID: " << task.id << "\n";
        for (int i = 0; i < 3; ++i) {
            std::cout << "  Phase " << i
                    << " | Required: " << task.phase[i].requiredTime
                    << " | Elapsed: " << task.phase[i].elapsedTime
                    << " | Energy_used: " << task.phase[i].energyUsed << "\n";
        }
    }
/*
Task ID: T_1
  Phase 0 | Required: 60 | Elapsed: 0 | Energy_used: 0
  Phase 1 | Required: 20 | Elapsed: 0 | Energy_used: 0
  Phase 2 | Required: 120 | Elapsed: 0 | Energy_used: 0
Task ID: T_2
  Phase 0 | Required: 60 | Elapsed: 0 | Energy_used: 0
  Phase 1 | Required: 20 | Elapsed: 0 | Energy_used: 0
  Phase 2 | Required: 120 | Elapsed: 0 | Energy_used: 0
Task ID: T_3
  Phase 0 | Required: 60 | Elapsed: 0 | Energy_used: 0
  Phase 1 | Required: 20 | Elapsed: 0 | Energy_used: 0
  Phase 2 | Required: 120 | Elapsed: 0 | Energy_used: 0
Task ID: T_4
  Phase 0 | Required: 60 | Elapsed: 0 | Energy_used: 0
  Phase 1 | Required: 20 | Elapsed: 0 | Energy_used: 0
  Phase 2 | Required: 120 | Elapsed: 0 | Energy_used: 0
*/


    int currentTaskIndex = 0;
    int t = 0;

    // open the output file to better view the logs
    std::ofstream outputFile("power_module_testing2.csv", std::ios::out);
    if (!outputFile.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return 1; 
    }
    outputFile << "Minute|,Orbit|,Battery|,Available|,Task|,Phase|,Time_Done|" << std::endl; // write the headers 

    while (t < SIM_DURATION && currentTaskIndex < tasks.size()) {
        Task& currentTask = tasks[currentTaskIndex];
        std::string orbitPhase = (t % 90 < 45) ? "sunlight" : "eclipse"; // orbit repeats every 90 mins with 45 minutes of sinlight

        // Power budget for this minute
        Power.update(t, orbitPhase);

        // Get the current phase info for the current task 
        Task::PhaseInfo& phase = currentTask.currentPhase();

        // set the current phase and required power for that phase
        int requiredPower = 0;
        std::string phaseName;

        if (currentTask.currentStage == 0) {
            requiredPower = 300; // from minutes [45-60) the available power is 0 since the deposition phase is using exactly the same power 
            phaseName = "deposition";
            //  DepositionModule::runOneMinute(currentTask, PowerModule, logger);
        } else if (currentTask.currentStage == 1) {
            requiredPower = 200;
            phaseName = "ion_implantation";
        } else if (currentTask.currentStage == 2) {
            requiredPower = 250;
            phaseName = "crystal_growth";
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

    std::cout << "Simulation complete.\n";
    return 0;
}

/*
OLD MANUAL LOOP SKETCH:

int currentTask = 0;
int phaseMinute = 0;
std::string currentPhase = "deposition";

for (int t = 0; t < SIM_DURATION; ++t) {
    std::string orbitPhase = (t % 90 < 45) ? "sunlight" : "eclipse";  // 45min cycles
    Power.update(t, orbitPhase);

    if (currentPhase == "deposition") {
        if (Power.canSatisfyDemand(300)) {
            Power.consumePower(300);
            DepositionModule.run(); // placeholder
            ++phaseMinute;
        }

        if (phaseMinute == 60) {
            currentPhase = "ion_implantation";
            phaseMinute = 0;
        }
    }

    else if (currentPhase == "ion_implantation") {
        if (Power.canSatisfyDemand(200)) {
            Power.consumePower(200);
            IonImplantationModule.run(); // placeholder
            ++phaseMinute;
        }

        if (phaseMinute == 20) {
            currentPhase = "crystal_growth";
            phaseMinute = 0;
        }
    }

    else if (currentPhase == "crystal_growth") {
        if (Power.canSatisfyDemand(250)) {
            Power.consumePower(250);
            CrystalGrowthModule.run(); // placeholder
            ++phaseMinute;
        }

        if (phaseMinute == 120) {
            currentTask++;
            phaseMinute = 0;
            currentPhase = "deposition";
        }
    }

    std::cout << "Minute " << t << " | Orbit Phase: " << orbitPhase
              << " | Battery: " << Power.getBatteryLevel()
              << " | Available: " << Power.getAvailablePower()
              << " | Task: " << currentTask
              << " | Phase: " << currentPhase << "\n";

    if (currentTask == 10) break; // Done with all wafers
}
*/
