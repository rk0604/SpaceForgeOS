#include "PowerBus.hpp"
#include <iostream>

PowerBus::PowerBus() : Subsystem("PowerBus"), available_power_(0.0) {}

void PowerBus::initialize() {
    available_power_ = 0.0;
}

void PowerBus::tick(const TickContext& ctx) {
    std::cout << "[PowerBus] Available power: " << available_power_ << " W\n";
    available_power_ = 0.0;
}

void PowerBus::shutdown() {
    std::cout << "[PowerBus] Shutdown.\n";
}

void PowerBus::addPower(double watts) {
    available_power_ += watts;
}

double PowerBus::drawPower(double requested) {
    double granted = (requested <= available_power_) ? requested : available_power_;
    available_power_ -= granted;
    return granted;
}

double PowerBus::getAvailablePower() const {
    return available_power_;
}