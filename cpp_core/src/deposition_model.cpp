#include "DepositionModule.hpp"
#include <iostream>
#include <algorithm>
#include <fstream> 
#include <cstdlib> // For rand(), srand()
#include <ctime>    // for time()

// Constructor
DepositionModule::DepositionModule() 
    : activeTask(nullptr), elapsed(0) 
{
    std::cout << "Called: DepositionModule::DepositionModule()" << std::endl;
}

// Enqueue task into queue
void DepositionModule::enqueue(const Task& task) {
    std::cout << "Called: DepositionModule::enqueue() | Task ID: " << task.id << std::endl;
    queue.push(task);
}

// One-minute update method
void DepositionModule::update(int t, PowerModule& power, Logger& logger) {
    std::cout << "Called: DepositionModule::update() | Minute: " << t << std::endl;
    // Stub: no actual behavior
}

// Check if current task is done
bool DepositionModule::hasCompletedTask() const {
    std::cout << "Called: DepositionModule::hasCompletedTask()" << std::endl;
    return (activeTask && activeTask->currentPhase().isDone());
}

// Pop the completed task
Task DepositionModule::popCompleted() {
    std::cout << "Called: DepositionModule::popCompleted()" << std::endl;
    Task completed = *activeTask;
    activeTask = nullptr;
    elapsed = 0;
    return completed;
}

// Static function to run one minute of deposition
void DepositionModule::runOneMinute(Task& task, PowerModule& power, Logger& logger) {
    std::ofstream file("deposition_debug_log.txt", std::ios::app);

    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open deposition_debug_log.txt" << std::endl;
        return;
    }

    file << "Called: DepositionModule::runOneMinute()\n";
    file << "  Task ID_DEP: " << task.id << "\n";
    file << "  Required Time_dep: " << task.phase[0].requiredTime << "\n";
    file << "  Battery Level_dep: " << power.getBatteryLevel() << "\n";
    file << "--------------------------\n";
    file.close();

    // task.phase[0].elapsedTime++; // keep progress count in main.cpp - one global clock

    // Generate a random number between 0 and 1
    double randomNumber = static_cast<double>(rand()) / RAND_MAX;

    if (randomNumber < task.phase[0].defectChance) {
        task.phase[0].defective = true;
    }
}


