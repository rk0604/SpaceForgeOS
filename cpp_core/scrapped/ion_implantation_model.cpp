/** PHILOSOPHY AND GUIDELINES
 * Why??: A truly concurrent module doesn’t need to know “what minute we’re at.”
 * It only needs to know “am I allowed to consume power right now and is the wafer ready for my stage?”
 */



#include "IonImplantationModule.hpp"
#include <iostream>
#include <algorithm>
#include <fstream> 
#include <cstdlib> // For rand(), srand()
#include <ctime>   // For time()
#include <mutex>
#include "Logger.hpp"
#include <atomic>

// constructor
IonImplantationModule:: IonImplantationModule():
    activeTask(nullptr), elapsed(0), COOL_DOWN(5), CALIBRATING_IMP_MODULE(true), CALIBRATION_TIME(3)// initialization list
    {   
        std::cout << "Called: IonImplantationModule::IonImplantationModule()" << std::endl;
    }

// Enqueue task into queue — now using pointer to avoid copying
void IonImplantationModule::enqueueIonImplantation(Task* task) {
    std::cout << "Called: IonImplantationModule::enqueue() | Task ID: " << task->id << std::endl;
    IonImplantQueue.push(task);  // store pointer to actual task from main
}

// check if queue inside ion implantation module is empty 
bool IonImplantationModule::IonImplantationModuleEmpty() { 
    return IonImplantQueue.empty(); 
}

// check if a task has been completed 
bool IonImplantationModule::hasCompletedTask() {
    COOL_DOWN = 5;
    return (activeTask && activeTask->phase[1].isDone());
}

// check if a task is calibrating 
bool IonImplantationModule::isCalibrating() const {  return CALIBRATING_IMP_MODULE;}

// check if the module is still cooling down
bool IonImplantationModule::isCoolingDown() { return COOL_DOWN > 0; }

// check if a task has been interrupted 
bool IonImplantationModule::imp_interrupted(){
    return (activeTask->phase[1].wasInterrupted);
}

// One-minute update method - owns the state machine of the module 
void IonImplantationModule::update_imp(int t, PowerModule& power, Logger& logger, std::mutex* powerMutex, std::atomic<int>* orbitState) {
    std::cout << "Called: IonImplantationModule::update() | Minute: " << t << std::endl;
    std::string orbit = orbitState -> load() == 0 ? "sunlight" : "eclipse";

    // If task is complete, pop it
    if (hasCompletedTask()) {
        Task* finished = popCompleted(); // COOL_DOWN is set to 5
        std::cout << "Task completed and removed from IonImplantationModule: " << finished->id << "\n";
        // Do not delete the task since main owns it
    }

    // skip minute if cooldown is active & decrement COOL_DOWN
    if (COOL_DOWN > 0) {
        if (activeTask) {
            std::cout << "Cannot use ion implantation module, remaining COOLDOWN: " << COOL_DOWN << " | Task: " << activeTask->id << "\n";
        } else {
            std::cout << "Cannot use ion implantation module, remaining COOLDOWN: " << COOL_DOWN << " | No active task\n";
        }
        COOL_DOWN--;
        return;
    }   

    // If no active task, try to get one from the queue & start calibration
    if (!activeTask && !IonImplantQueue.empty()) {
        activeTask = IonImplantQueue.front();
        IonImplantQueue.pop();
            CALIBRATING_IMP_MODULE = true;
            CALIBRATION_TIME = 3; 
        std::cout << "Started new task: " << activeTask->id << "Calibrating: " << CALIBRATION_TIME << "\n";
    }

    // If there's an active task, try to run it
    if (activeTask) {
        int requiredPower = 200; 

        // calibration 
        if (CALIBRATING_IMP_MODULE && CALIBRATION_TIME > 0) {
            requiredPower = 100;
            // power mutex lock
            {
                std::lock_guard<std::mutex> powerLockCalibration(*powerMutex);
                if (!power.canSatisfyDemand(100)) {
                    std::cout << "Power interruption! Task " << activeTask->id << " marked defective.\n";
                    return;
                }
                power.consumePower(100);
            }

            // task phase mutex
            {
                std::lock_guard<std::mutex> phaseLock(activeTask->phaseMutex[1]);
                activeTask->phase[1].energyUsed += requiredPower;
                activeTask->phase[1].elapsedTime++;
            }

            CALIBRATION_TIME--;
            if (CALIBRATION_TIME == 0) CALIBRATING_IMP_MODULE = false;
            return;
        }

        bool enoughPower = false;
        { // second PowerLock to see if powerConsumption can be done and actually consume power  
            std::lock_guard <std::mutex> powerLockConsumtion(*powerMutex);
            if (power.canSatisfyDemand(requiredPower)) {
                power.consumePower(requiredPower);    
                enoughPower = true;             
            }
        }
        {
            std::lock_guard<std::mutex> phaseLock(activeTask -> phaseMutex[1]);
            if(enoughPower){
                activeTask -> phase[1].energyUsed += requiredPower;
                runOneMinute_imp(*activeTask, power, logger);  
                activeTask -> phase[1].elapsedTime++;
            } else{
                activeTask -> phase[1].wasInterrupted = true;
                activeTask->phase[1].defective = true; // CANNOT have ion implantation stop
                activeTask -> phase[1].elapsedTime++; // maybe change 
                std::cout << "Not enough power ION IMP, skipping this task this minute.\n";
            }
        }
    }

        logger.log(
        t,
        "ION",
        activeTask->id,
        1,
        true,
        CALIBRATING_IMP_MODULE,
        COOL_DOWN,
        activeTask->phase[1].elapsedTime,
        activeTask->phase[1].requiredTime,
        activeTask->phase[1].energyUsed,
        power.getBatteryLevel() / 1000,
        power.getAvailablePower(),
        activeTask->phase[1].wasInterrupted,
        activeTask->phase[1].defective,
        orbit,  // You'll need to pass orbit string into update()
        "run",
        0.0f
    );
}


// Static function to run one minute of IonImplantationModule
// Static because it doesn't use any internal members of the IonImplantationModule class
void IonImplantationModule::runOneMinute_imp(Task& task, PowerModule& power, Logger& logger) {
    std::ofstream file("debugLogs/IonImplantationModule_debug_log.txt", std::ios::app);

    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open IonImplantationModule_debug_log.txt" << std::endl;
        return;
    }

    file << "Called: IonImplantationModuleModule::runOneMinute()\n";
    file << "  Task ID_DEP: " << task.id << "\n";
    file << "  Required Time_to_completion: " 
         << (task.phase[1].requiredTime - task.phase[1].elapsedTime) << "\n";
    file << "  Battery levels_post_exec: " << power.getBatteryLevel() << "\n";
    file << "--------------------------\n";
    file.close();

    // Generate a random number between 0 and 1
    double randomNumber = static_cast<double>(rand()) / RAND_MAX;

    if (randomNumber < task.phase[1].defectChance) {
        task.phase[1].defective = true;
    }
}

// Pop the completed task
Task* IonImplantationModule::popCompleted() {
    std::cout << "Called: Ion::popCompleted()" << std::endl;
    Task* completed = activeTask;  // just return pointer, no copy
    activeTask = nullptr;          // machine is now idle
    elapsed = 0;
    COOL_DOWN = 5; // RESET 5 minute cooldown
    return completed;
}

// remove the discarded task, takes in the active task 
void IonImplantationModule::discardTask_imp(Task* task) {
    std::cout << "[IonImplantationModule] Discarding Task: " << task->id << "\n";

    // If the task is currently active, reset the module state
    if (activeTask == task) {
        std::cout << "[IonImplantationModule] Task was active. Resetting active slot.\n";
        activeTask = nullptr;
        elapsed = 0;
        CALIBRATING_IMP_MODULE = false;
        CALIBRATION_TIME = 0;
        COOL_DOWN = 0;
    }

    // Remove task from the queue if it's still in there
    std::queue<Task*> newQueue;
    while (!IonImplantQueue.empty()) {
        Task* queuedTask = IonImplantQueue.front();
        IonImplantQueue.pop();
        if (queuedTask != task) {
            newQueue.push(queuedTask);
        } else {
            std::cout << "[IonImplantationModule] Task found in queue and removed: " << task->id << "\n";
        }
    }

    IonImplantQueue.swap(newQueue);  // replace with filtered queue
}

