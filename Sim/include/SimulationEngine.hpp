#pragma once
#include "Subsystem.hpp"
#include <vector>
#include "TickContext.hpp"
#include "TelemetryLogger.hpp"

class Battery;
class SolarArray;
class PowerBus;

class SimulationEngine {
public:
    void addSubsystem(Subsystem* subsystem);
    void initialize();
    void tick();
    void shutdown();
    void setTickStep(double dt);

private:
    TelemetryLogger logger_{"../../data/raw/telemetry.csv"};
    std::vector<Subsystem*> subsystems_;

    Battery* battery_ = nullptr;
    SolarArray* solar_ = nullptr;
    PowerBus* powerbus_ = nullptr;


    int tick_count_ = 0;
    double sim_time_ = 0.0;
    double tick_step_ = 0.1;
};
