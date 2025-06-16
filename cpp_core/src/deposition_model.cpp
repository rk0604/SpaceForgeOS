#include "DepositionModule.hpp"
#include <iostream>
#include <algorithm>
#include <fstream> 
#include <cstdlib> // For rand(), srand()
#include <ctime>   // For time()

// Constructor
DepositionModule::DepositionModule() 
    : activeTask(nullptr), elapsed(0) 
{
    std::cout << "Called: DepositionModule::DepositionModule()" << std::endl;
}

// Enqueue task into queue — now using pointer to avoid copying
void DepositionModule::enqueue(Task* task) {
    std::cout << "Called: DepositionModule::enqueue() | Task ID: " << task->id << std::endl;
    queue.push(task);  // store pointer to actual task from main
}

// check if queue inside deposition module is empty 
bool DepositionModule::DepositionModuleEmpty() { 
    return queue.empty(); 
}

// check if a task has been completed 
bool DepositionModule::hasCompletedTask() {
    return (activeTask && activeTask->phase[0].isDone());
}

// One-minute update method - owns the state machine of the module 
void DepositionModule::update(int t, PowerModule& power, Logger& logger) {
    std::cout << "Called: DepositionModule::update() | Minute: " << t << std::endl;

    // If task is complete, pop it
    if (hasCompletedTask()) {
        Task* finished = popCompleted();
        std::cout << "Task completed and removed from DepositionModule: " << finished->id << "\n";
        // Do not delete the task since main owns it
    }

    // If no active task, try to get one from the queue
    if (!activeTask && !queue.empty()) {
        activeTask = queue.front(); // just point to the actual task
        queue.pop(); // remove from queue
        std::cout << "Started new task: " << activeTask->id << "\n";
    }

    // If there's an active task, try to run it
    if (activeTask) {
        int requiredPower = 300; 

        if (power.canSatisfyDemand(requiredPower)) {
            power.consumePower(requiredPower);
            activeTask -> phase[0].energyUsed += requiredPower;
            runOneMinute(*activeTask, power, logger);  
            activeTask->phase[0].elapsedTime++;                 
        } else {
            activeTask -> phase[0].wasInterrupted = true;
            activeTask->phase[0].elapsedTime++; 
            std::cout << "Not enough power, skipping this task this minute.\n";
        }
    }
}

// Static function to run one minute of deposition
// Static because it doesn't use any internal members of the DepositionModule class
void DepositionModule::runOneMinute(Task& task, PowerModule& power, Logger& logger) {
    std::ofstream file("deposition_debug_log.txt", std::ios::app);

    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open deposition_debug_log.txt" << std::endl;
        return;
    }

    file << "Called: DepositionModule::runOneMinute()\n";
    file << "  Task ID_DEP: " << task.id << "\n";
    file << "  Required Time_to_completion: " 
         << (task.phase[0].requiredTime - task.phase[0].elapsedTime) << "\n";
    file << "  Battery levels_post_exec: " << power.getBatteryLevel() << "\n";
    file << "--------------------------\n";
    file.close();

    // Generate a random number between 0 and 1
    double randomNumber = static_cast<double>(rand()) / RAND_MAX;

    if (randomNumber < task.phase[0].defectChance) {
        task.phase[0].defective = true;
    }
}

// Pop the completed task
Task* DepositionModule::popCompleted() {
    std::cout << "Called: DepositionModule::popCompleted()" << std::endl;
    Task* completed = activeTask;  // just return pointer, no copy
    activeTask = nullptr;          // machine is now idle
    elapsed = 0;
    return completed;
}
