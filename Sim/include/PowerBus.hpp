#pragma once
#include "Subsystem.hpp"
#include <vector>

class PowerBus : public Subsystem {
public:
    PowerBus();
    void initialize() override;
    void tick(const TickContext& ctx) override;
    void shutdown() override;

    void addPower(double watts);
    double drawPower(double requested);
    double getAvailablePower() const;

private:
    double available_power_;
};