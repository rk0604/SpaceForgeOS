#include "Logger.hpp"
#include <iostream>

// Optional: actually increment throughput count
void Logger::incrementThroughput() {
    throughput++;
}

// Just print out when log is called — stub for testing
void Logger::log(int t,
                 const DepositionModule&,
                 const IonImplantationModule&,
                 const CrystalGrowthModule&,
                 const PowerModule& power,
                 const std::string& orbitalPhase) {
    std::cout << "[Logger] Time: " << t << ", Orbit Phase: " << orbitalPhase
              << ", Battery: " << power.getBatteryLevel() << std::endl;
}

// Placeholder — no actual CSV logic yet
void Logger::exportCSV(const std::string& filename) {
    std::cout << "[Logger] Exporting CSV to: " << filename << std::endl;
}
