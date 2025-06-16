#ifndef IONIMPLANTATIONMODULE_HPP
#define IONIMPLANTATIONMODULE_HPP

#include "Task.hpp"
#include "PowerModule.hpp"
#include "Logger.hpp"
#include <queue>

class IonImplantationModule {
private:
    std::queue<Task*> IonImplantQueue; // queue of pending wafer tasks to be processed 
    Task* activeTask = nullptr;
    int elapsed = 0;

public:
    // empty constructor 
    IonImplantationModule(); 

    void enqueue(Task* task);

    bool IonImplantationModuleEmpty();

    bool hasCompletedTask();

    Task* popCompleted();
    
    // not static because otherwise cannot use class instance members like this, queue, or activeTask 
    // not const because it cannot modify members 
    void update(int t, PowerModule& power, Logger& logger);

    // ion implantation's version of runOneMinute
    static void runOneMinute_imp(Task& task, PowerModule& power, Logger& logger);

};

/**
 * Implantation is about total dose — number of ions implanted per cm². It’s not just time-based; total energy delivered matters.
 * - Directional shielding requiremenT
 * - Retry mechanism on failure
 * - Random drift chance per minute
 */



 #endif

