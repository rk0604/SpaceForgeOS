#include "TelemetryLogger.hpp"

TelemetryLogger::TelemetryLogger(const std::string& filename) {
    file_.open(filename);
    file_ << "tick,time,battery_charge,solar_output,powerbus_available\n";
}

TelemetryLogger::~TelemetryLogger() {
    if (file_.is_open())
        file_.close();
}

void TelemetryLogger::log(int tick, double time, double battery, double solar, double bus) {
    file_ << tick << "," << time << "," << battery << "," << solar << "," << bus << "\n";
}
