#ifndef DEPOSITION_MODULE_HPP
#define DEPOSITION_MODULE_HPP

#include "Task.hpp"
#include "PowerModule.hpp"
#include "Logger.hpp"
#include <queue>
#include <mutex>
#include <atomic>

/**
 * @brief Represents a deposition machine that processes wafer tasks minute-by-minute.
 * Internally uses a queue of task pointers to simulate sequential real-time job processing.
 */
class DepositionModule {
private:
    std::queue<Task*> queue;     ///< Queue of wafer tasks waiting to be processed
    Task* activeTask = nullptr;  ///< Currently running task (nullptr if idle)
    int elapsed = 0;             ///< Tracks elapsed time for the current task

public:
    /**
     * @brief Default constructor. Initializes with no active task.
     * 
     * @note Not marked `explicit` since no risk of unintended conversions.
     *       No parameters needed, so a plain constructor is sufficient.
     */
    DepositionModule(); 

    /**
     * @brief Adds a new task to the internal queue (non-copying).
     * 
     * @param task Pointer to Task. Stored directly to avoid deep copies and preserve task state.
     * 
     * @note Uses pointer-based enqueueing to ensure state changes in the original object
     *       are reflected system-wide (since task is shared across modules).
     */
    void enqueue(Task* task);

    /**
     * @brief Checks whether the queue is currently empty.
     * 
     * @return true if no tasks are waiting; false otherwise.
     * 
     * @note Not marked `const` since it doesn't affect object state,
     *       but can be improved by doing so.
     */
    bool DepositionModuleEmpty();

    /**
     * @brief Main function to simulate one minute of real-time operation.
     * 
     * Handles task selection, power validation, task processing, and interruption.
     * If current task finishes, prepares to return it. If power is insufficient,
     * marks the task as interrupted.
     * 
     * @param t      Current simulation time (in minutes)
     * @param power  Reference to power system to check and deduct energy
     * @param logger Reference to global logger for task event tracking
     * 
     * @note Not marked `static` because it modifies internal state.
     *       Not marked `const` since it alters members like `activeTask`, `queue`, etc.
     */
    void update(int t, PowerModule& power, Logger& logger, std::mutex* powerMutex, std::atomic<int>* orbitState);

    /**
     * @brief Determines whether the current active task has finished processing.
     * 
     * @return true if a task is currently running and its phase[0] is done.
     *         false otherwise.
     * 
     * @note Helps coordinate task popping and logging.
     */
    bool hasCompletedTask();

    /**
     * @brief Returns the completed task and resets module state.
     * 
     * @return Pointer to the completed Task (not deleted — caller retains ownership).
     * 
     * @note Does not deallocate memory. `main.cpp` or controller is responsible for task lifetime.
     *       `activeTask` is set to `nullptr`, marking the machine as idle again.
     */
    Task* popCompleted();

    void discardTask_dep(Task* task);

    /**
     * @brief Static helper function to simulate one minute of processing for a task.
     * 
     * Logs debug info to file and probabilistically marks the task as defective.
     * 
     * @param task   Reference to task being processed
     * @param power  Reference to power manager (to log post-power state)
     * @param logger Logger to track simulation progress (currently not used here, but passed for consistency)
     * 
     * @note Marked `static` because it operates only on the passed-in task and not on any instance variables.
     */
    static void runOneMinute(Task& task, PowerModule& power, Logger& logger);
};

#endif
