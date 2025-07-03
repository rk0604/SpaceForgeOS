#include "DepositionModule.hpp"
#include <iostream>
#include <algorithm>
#include <fstream> 
#include <cstdlib> // For rand(), srand()
#include <ctime>   // For time()
#include <mutex>
#include "Logger.hpp"
#include <atomic>

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
void DepositionModule::update(int t, PowerModule& power, Logger& logger, std::mutex* powerMutex, std::atomic<int>* orbitState) {
    std::cout << "Called: DepositionModule::update() | Minute: " << t << std::endl;
    std::string orbit = orbitState -> load() == 0 ? "sunlight" : "eclipse";

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
        {
            // Lock powerMutex ONLY around power operations: lock_guard needs a name to instantiate; <std::mutex> is a template specialization "typecast"
            std::lock_guard<std::mutex> powerLock(*powerMutex);

            if (power.canSatisfyDemand(requiredPower)) {
                power.consumePower(requiredPower);
            } else {
                {
                    std::lock_guard<std::mutex> lockPhaseDep(activeTask->phaseMutex[0]);
                    activeTask->phase[0].wasInterrupted = true;
                    activeTask->phase[0].elapsedTime++;
                }
                std::cout << "Not enough power, skipping this task this minute.\n";
                return;
            }
        }   // mutex is unlocked 

        // Now lock the task phase safely to increment ITS elapsedTime and energyUsed
        {
            std::lock_guard<std::mutex> lockPhaseDep(activeTask -> phaseMutex[0]);
            activeTask -> phase[0].energyUsed += requiredPower;
            runOneMinute(*activeTask, power, logger);
            activeTask -> phase[0].elapsedTime++;
        }

        logger.log(
            t,
            "Deposition",
            activeTask->id,
            0,
            true,
            false,
            0,
            activeTask->phase[0].elapsedTime,
            activeTask->phase[0].requiredTime,
            activeTask->phase[0].energyUsed,
            power.getBatteryLevel() / 1000,
            power.getAvailablePower(),
            activeTask->phase[0].wasInterrupted,
            activeTask->phase[0].defective,
            orbit,  // You'll need to pass orbit string into update()
            "run",
            0.0f
        );
    }
}


// Static function to run one minute of deposition
// Static because it doesn't use any internal members of the DepositionModule class
void DepositionModule::runOneMinute(Task& task, PowerModule& power, Logger& logger) {
    std::ofstream file("debugLogs/deposition_debug_log.txt", std::ios::app);

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

// Discard a task from the active slot and internal queue
void DepositionModule::discardTask_dep(Task* task) {
    std::cout << "[DepositionModule] Discarding Task: " << task->id << std::endl;

    // If the task is currently being processed
    if (activeTask == task) {
        std::cout << "[DepositionModule] Task was active. Resetting active slot.\n";
        activeTask = nullptr;
        elapsed = 0;
    }

    // Rebuild the queue without the discarded task
    std::queue<Task*> newQueue;
    while (!queue.empty()) {
        Task* queuedTask = queue.front();
        queue.pop();
        if (queuedTask != task) {
            newQueue.push(queuedTask);
        } else {
            std::cout << "[DepositionModule] Task found in queue and removed: " << task->id << "\n";
        }
    }

    queue.swap(newQueue);  // Replace with updated queue
}
