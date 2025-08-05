#include "SolarArray.hpp"
#include "PowerBus.hpp"
#include <cmath>
#include <iostream>

SolarArray::SolarArray() : Subsystem("SolarArray"), bus_(nullptr), efficiency_(0.2) {}

void SolarArray::setPowerBus(PowerBus* bus) {
    bus_ = bus;
}

void SolarArray::initialize() {
    std::cout << "[SolarArray] Initialized.\n";
}

double SolarArray::getLastOutput() const {
    return last_output_;
}

void SolarArray::tick(const TickContext& ctx) {
    double solar_input = 1000.0 * std::fabs(std::cos(ctx.time)); // basic orbit cosine model
    double output = solar_input * efficiency_;
    last_output_ = output;  // ✅ track output for logging

    std::cout << "[SolarArray] Generated: " << output << " W\n";

    if (bus_) bus_->addPower(output);
}


void SolarArray::shutdown() {
    std::cout << "[SolarArray] Shutdown.\n";
}