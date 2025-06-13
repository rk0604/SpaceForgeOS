#ifndef DEPOSITION_MODULE_HPP
#define DEPOSITION_MODULE_HPP

#include "Task.hpp"
#include "PowerModule.hpp"
#include "Logger.hpp"
#include <queue>

/**
 * @brief Represents a deposition machine that can process multiple wafers (Tasks) using a queue.
 * Each task runs minute-by-minute based on available power, simulating real-time behavior.
 */
class DepositionModule {
private:
    std::queue<Task> queue;   // Queue of pending wafer tasks to be processed
    Task* activeTask = nullptr;  // Pointer to the current task being worked on
    int elapsed = 0;             // Tracks how long the current task has been running

public:
    /**
     * @brief Constructor to initialize an empty DepositionModule
     */
    DepositionModule(); 

    /**
     * @brief Adds a new wafer (task) to the processing queue.
     * @param task The Task to be enqueued for deposition.
     */
    void enqueue(const Task& task);

    /**
     * @brief Runs one simulation step (one minute) for the active task.
     * If no task is active, pulls the next from the queue.
     * If task completes, resets the active slot.
     * @param t Current simulation time (in minutes)
     * @param power Reference to the PowerModule to check/consume energy
     * @param logger Reference to the logger for tracking task status
     */
    void update(int t, PowerModule& power, Logger& logger);

    /**
     * @brief Checks if the current active task has completed all its phases.
     * @return true if a completed task is ready to be retrieved; false otherwise
     */
    bool hasCompletedTask() const;

    /**
     * @brief Returns the completed task and resets the active task pointer.
     * @return The Task object that just finished deposition
     */
    Task popCompleted();

    /**
     * @brief Static helper function to perform 1 minute of deposition on a given task.
     * Useful when not using queues (e.g., single-task loop).
     * @param task Task being processed
     * @param power Reference to power manager for consumption logic
     * @param logger Reference to log progress/wait state
     */
    static void runOneMinute(Task& task, PowerModule& power, Logger& logger);
};

#endif
