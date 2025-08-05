#pragma once
#include "Subsystem.hpp"
class PowerBus;

class Battery : public Subsystem {
public:
    Battery();
    void setPowerBus(PowerBus* bus);
    


    void initialize() override;
    void tick(const TickContext& ctx) override;
    void shutdown() override;

    double discharge(double watts);
    double getCharge() const;
private:
    PowerBus* bus_;
    double capacity_;
    double charge_;
    double max_draw_rate_;
};
