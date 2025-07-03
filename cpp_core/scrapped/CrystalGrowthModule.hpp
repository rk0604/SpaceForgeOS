#ifndef CRYSTAL_GROWTH_MODULE_HPP
#define CRYSTAL_GROWTH_MODULE_HPP

#include "Task.hpp"
#include "PowerModule.hpp"
#include <queue>

class CrystalGrowthModule {
private:
    std::queue<Task> queue;
    Task* activeTask = nullptr;
    int elapsed = 0;

public:
    void enqueue(const Task& task);
    void update(int t, const PowerModule& power);
    bool hasCompletedTask() const;
    Task popCompleted();
};

#endif
