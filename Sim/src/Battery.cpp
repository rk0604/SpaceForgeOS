#include "Battery.hpp"
#include "PowerBus.hpp"
#include <iostream>

Battery::Battery() : Subsystem("Battery"), bus_(nullptr), capacity_(1000.0), charge_(500.0), max_draw_rate_(50.0) {}

void Battery::setPowerBus(PowerBus* bus) {
    bus_ = bus;
}

void Battery::initialize() {
    std::cout << "[Battery] Initialized with charge: " << charge_ << " Wh\n";
}

double Battery::getCharge() const {
    return charge_;
}

void Battery::tick(const TickContext& ctx) {
    if (!bus_) return;
    double required = max_draw_rate_ * ctx.dt;
    double drawn = bus_->drawPower(required);
    charge_ += drawn;
    if (drawn < required) {
        double deficit = required - drawn;
        charge_ -= deficit;
        std::cout << "[Battery] Drew: " << drawn << " W, deficit: " << deficit << " W\n";
    } else {
        std::cout << "[Battery] Fully charged this tick with " << drawn << " W\n";
    }
    charge_ = std::max(0.0, std::min(charge_, capacity_));
    if (charge_ < 50.0) {
    std::cout << "[Battery] ⚠️ Low charge! (" << charge_ << " Wh remaining)\n";
    }
}


double Battery::discharge(double watts) {
    double provided = (watts <= charge_) ? watts : charge_;
    charge_ -= provided;
    std::cout << "[Battery] Discharged: " << provided << " W (Remaining: " << charge_ << " Wh)\n";
    return provided;
}


void Battery::shutdown() {
    std::cout << "[Battery] Shutdown. Final charge: " << charge_ << " Wh\n";
}