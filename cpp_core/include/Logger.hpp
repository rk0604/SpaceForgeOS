#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fstream>
#include <string>
#include <mutex>
#include <atomic>

class Logger {
private:
    std::ofstream file;
    std::mutex logMutex;
    int throughput = 0;

public:
    Logger(const std::string& filename = "logV1.csv");
    ~Logger();

    void incrementThroughput();
    int getThroughput() const;

    void log(int minute,
             const std::string& module,
             const std::string& taskId,
             int phaseIndex,
             bool isActive,
             bool isCalibrating,
             int cooldownRemaining,
             int elapsedTime,
             int requiredTime,
             int energyUsed,
             int batteryLevel,
             int powerAvailable,
             bool wasInterrupted,
             bool defective,
             const std::string& orbit,
             const std::string& action,
             float reward = 0.0f);
};

#endif
