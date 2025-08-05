#pragma once
#include "Subsystem.hpp"
class PowerBus;

class SolarArray : public Subsystem {
public:
    SolarArray();
    void setPowerBus(PowerBus* bus);

    void initialize() override;
    void tick(const TickContext& ctx) override;
    void shutdown() override;

    double getLastOutput() const;

private:
    PowerBus* bus_;
    double efficiency_;
    double last_output_ = 0.0;
};
