#include "Logger.hpp"
#include <iostream>
#include <mutex>
#include <atomic>

Logger::Logger(const std::string& filename) {
    file.open(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening log file: " << filename << std::endl;
        exit(1);
    }

    // Unified header for deep learning
    file << "Minute,Module,TaskID,Phase,Active,Calibrating,Cooldown,Elapsed,Required,EnergyUsed,BatteryLevel,PowerAvailable,Interrupted,Defective,Orbit,Action,Reward\n";
}

Logger::~Logger() {
    if (file.is_open()) {
        file.close();
    }
}

void Logger::incrementThroughput() {
    throughput++;
}

int Logger::getThroughput() const {
    return throughput;
}

void Logger::log(int minute,
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
                 float reward) {
    std::lock_guard<std::mutex> lock(logMutex);

    file << minute << ","
         << module << ","
         << taskId << ","
         << phaseIndex << ","
         << isActive << ","
         << isCalibrating << ","
         << cooldownRemaining << ","
         << elapsedTime << ","
         << requiredTime << ","
         << energyUsed << ","
         << batteryLevel << ","
         << powerAvailable << ","
         << wasInterrupted << ","
         << defective << ","
         << orbit << ","
         << action << ","
         << reward << "\n";
}
