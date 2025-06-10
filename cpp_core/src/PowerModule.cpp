#include "PowerModule.hpp"
#include <algorithm>  
// This file implements all the functions declared in the header

/* ---------- constructor ---------- */
PowerModule::PowerModule(int maxBattery,
                         int genSunlight,
                         int genEclipse)
    : battery_(maxBattery),
      maxBattery_(maxBattery),
      genSunlight_(genSunlight),
      genEclipse_(genEclipse),
      budgetThisMinute_(0),
      producedThisMinute_(0)    // ← start at zero
{}
// When a PowerModule object is created it starts with a full battery.
// genSunlight_ and genEclipse_ store how much the panels can generate during the sunlight and eclipse durations of the orbit 

/* ---------- private helper that picks the right wattage based on storage space  ---------- */
int PowerModule::solarGeneration(const std::string& phase) const {
    return (phase == "sunlight") ? genSunlight_ : genEclipse_;
}

/* ---------- public methods ---------- */
// adds generated power to the battery and sets how much power is available for this minute

void PowerModule::update(int t, const std::string& orbitalPhase) {

    producedThisMinute_ = solarGeneration(orbitalPhase); // 300W in sunlight, 0W in eclipse
    battery_ = std::min(std::max(battery_ + producedThisMinute_, 0), maxBattery_); // recharge battery up to max capacity 

    constexpr int maxDrawPerMin_Allowed = 300;                     // W you’ll allow from battery
    int batteryDrawPotential = std::min(maxDrawPerMin_Allowed, battery_);  // can either draw the amount available <300 or just 300
    budgetThisMinute_ = producedThisMinute_ + batteryDrawPotential; 
}

// returns true if there is enough power in this minute's budget
bool PowerModule::canSatisfyDemand(int watts) const {
    return watts <= budgetThisMinute_;
}

// actually consumes power for a device/task
// Even though you pack them together in one “budget,” that subtraction-and-difference step guarantees solar is “spent” first, then battery.
void PowerModule::consumePower(int watts) {
    budgetThisMinute_ -= watts;  // subtract from the available budget (the shared pool of solarGenerated + battery)
    int wattsFromBattery = std::max(0, watts - producedThisMinute_);  // if watts <= productedThisMinute_ by solar then no from battery
    battery_ = std::max(0, battery_ - wattsFromBattery); // else if watts > producedThisMinute_ then pull from battery 
    
}

// query methods
int PowerModule::getAvailablePower() const { return budgetThisMinute_; }
int PowerModule::getBatteryLevel()   const { return battery_; }


/*
PowerModule simulates a satellite’s energy system with two sources:
  1. Solar Panels (primary)
  2. Battery (secondary)

Each minute, when update() is called:

1. **Solar Charging Phase**
   - Determine actual panel output based on orbital phase:
     - 'sunlight' → genSunlight_ watts
     - 'eclipse'  → genEclipse_ watts (often 0)
   - Store that in producedThisMinute_.
   - Add producedThisMinute_ to battery_, clamped to [0, maxBattery_].

2. **Setting the Power Budget**
   - Budget = producedThisMinute_ (all solar power)
            + up to maxDrawPerMin (300W) drawn from battery_.
   - This budget is what tasks can pull from this minute.

3. **Using the Power (consumePower)**
   - A task checks canSatisfyDemand(watts) against the budget.
   - If approved:
     - Subtract watts from budgetThisMinute_.
     - Anything above producedThisMinute_ is drawn from battery_:
         fromBattery = max(0, watts – producedThisMinute_)
     - Subtract fromBattery from battery_, never going below zero.

This design ensures:
- **Solar power is used first**—instant, free, and replenishing the battery.
- **Battery is backup**, limited to 300W draw per minute.
- **Realistic eclipse behavior:** when producedThisMinute_ is zero,
  tasks draw solely from battery (up to the draw cap), and the battery
  depletes accordingly. During sunlight, battery recharges up to maxBattery_.
*/
