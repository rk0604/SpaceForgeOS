#include "SimulationEngine.hpp"
#include "Battery.hpp"
#include "SolarArray.hpp"
#include "PowerBus.hpp"

int main() {
    PowerBus bus;
    SolarArray solar;
    Battery battery;

    solar.setPowerBus(&bus);
    battery.setPowerBus(&bus);

    SimulationEngine engine;
    engine.addSubsystem(&solar);
    engine.addSubsystem(&bus);
    engine.addSubsystem(&battery);

    engine.initialize();
    engine.setTickStep(0.1); // optional
    for (int i = 0; i < 50; ++i)
    engine.tick(); 

    return 0;
}