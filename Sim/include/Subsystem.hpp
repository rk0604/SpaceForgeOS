#pragma once
#include <string>
#include "TickContext.hpp"

class Subsystem {
public:
    explicit Subsystem(const std::string& name) : name_(name) {}
    virtual ~Subsystem() = default;

    // Called once at sim startup
    virtual void initialize() = 0;
    // Called every tick with fixed time step (seconds)
    virtual void tick(const TickContext& ctx) = 0;
    // Called once at sim shutdown
    virtual void shutdown() = 0;

    const std::string& getName() const { return name_; }

protected:
    std::string name_;
};