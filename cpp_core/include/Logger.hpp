#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include "PowerModule.hpp" // you still need full definition here

// Forward declarations — just enough for pointers/references
class DepositionModule;
class IonImplantationModule;
class CrystalGrowthModule;

class Logger {
private:
    int throughput = 0;

public:
    void incrementThroughput();
    void log(int t,
             const DepositionModule& d,
             const IonImplantationModule& i,
             const CrystalGrowthModule& c,
             const PowerModule& power,
             const std::string& orbitalPhase);
    void exportCSV(const std::string& filename);
};

#endif
