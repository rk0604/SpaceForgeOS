#include "SimulationEngine.hpp"
#include "Battery.hpp"
#include "PowerBus.hpp"
#include "SolarArray.hpp"
#include "TelemetryLogger.hpp"
#include <iostream>

void SimulationEngine::addSubsystem(Subsystem* subsystem) {
    subsystems_.push_back(subsystem);
}

void SimulationEngine::initialize() {
    solar_ = new SolarArray();
    powerbus_ = new PowerBus();
    battery_ = new Battery();

    solar_->setPowerBus(powerbus_);
    battery_->setPowerBus(powerbus_);

    subsystems_.push_back(solar_);
    subsystems_.push_back(battery_);
    subsystems_.push_back(powerbus_);

    for (auto* s : subsystems_) {
        s->initialize();
    }
}

void SimulationEngine::tick() {
    TickContext ctx {
        .tick_index = tick_count_,
        .time = sim_time_,
        .dt = tick_step_
    };

    std::cout << "[Tick " << tick_count_ << "] t = " << sim_time_ << " s\n";
    // Run all subsystem updates
    solar_->tick(ctx);     // generate
    battery_->tick(ctx);   // consume
    powerbus_->tick(ctx);  // reset

    logger_.log(tick_count_, sim_time_, battery_->getCharge(), solar_->getLastOutput(), powerbus_->getAvailablePower());

    // Advance sim time
    tick_count_++;
    sim_time_ += tick_step_;

}

void SimulationEngine::setTickStep(double dt) {
    tick_step_ = dt;
}


void SimulationEngine::shutdown() {
    for (auto* s : subsystems_) s->shutdown();
}