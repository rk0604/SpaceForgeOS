#ifndef IONIMPLANTATIONMODULE_HPP
#define IONIMPLANTATIONMODULE_HPP

#include "Task.hpp"
#include "PowerModule.hpp"
#include "Logger.hpp"
#include <queue>
#include <mutex>
#include <atomic>

class IonImplantationModule {
private:
    std::queue<Task*> IonImplantQueue; // queue of pending wafer tasks to be processed 
    Task* activeTask = nullptr;
    int elapsed = 0;
    int COOL_DOWN = 0;
    bool CALIBRATING_IMP_MODULE = true;
    int CALIBRATION_TIME = 3;

public:
    // empty constructor 
    IonImplantationModule(); 

    void enqueueIonImplantation(Task* task);

    bool IonImplantationModuleEmpty();

    bool hasCompletedTask();

    Task* popCompleted();
    
    // not static because otherwise cannot use class instance members like this, queue, or activeTask 
    // not const because it cannot modify members 
    void update_imp(int t, PowerModule& power, Logger& logger, std::mutex* powerMutex, std::atomic<int>* orbitState);

    bool isCalibrating() const;

    bool isCoolingDown();
    
    bool imp_interrupted();

    // ion implantation's version of runOneMinute
    static void runOneMinute_imp(Task& task, PowerModule& power, Logger& logger);

    void discardTask_imp(Task *task);

};

/**
 * Implantation is about total dose — number of ions implanted per cm². It’s not just time-based; total energy delivered matters.
 * - Directional shielding requiremenT
 * - Retry mechanism on failure
 * - Random drift chance per minute
 */


#endif

