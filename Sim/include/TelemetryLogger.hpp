#pragma once
#include <fstream>
#include <string>

class TelemetryLogger {
public:
    TelemetryLogger(const std::string& filename);
    ~TelemetryLogger();

    void log(int tick, double time, double battery, double solar, double bus);

private:
    std::ofstream file_;
};
