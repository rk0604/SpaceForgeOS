#ifndef ION_IMPLANTATION_MODULE_HPP
#define ION_IMPLANTATION_MODULE_HPP

#include "Task.hpp"
#include "PowerModule.hpp"
#include <queue>

class IonImplantationModule {
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
