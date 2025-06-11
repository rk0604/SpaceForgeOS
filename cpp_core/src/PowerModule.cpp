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
 In the log above, you can see what happens when we cross from sunlight into eclipse:

 Minute 44 (sunlight):
   - producedThisMinute_ = 300 W
   - battery_ was 1000 mWh, so batteryDrawPotential = min(300,1000) = 300 W
   - budgetThisMinute_ = 300 (solar) + 300 (battery) = 600 W
   - consumePower(300) uses 300 W solar first, then none from battery
   - Battery remains 1000 mWh, Available drops to 300 W, deposition ⟶ 45/60 ✅

 Minute 45 (first minute of eclipse):
   - producedThisMinute_ = 0 W
   - battery_ is still 1000 mWh, so batteryDrawPotential = min(300,1000) = 300 W
   - budgetThisMinute_ = 0 (solar) + 300 (battery) = 300 W
   - consumePower(300) uses 0 W solar, then 300 W from battery
   - Battery decreases 1000–300 = 700 mWh, Available drops to 0 W, deposition ⟶ 46/60 ✅

 Minute 46 (eclipse):
   - producedThisMinute_ = 0 W
   - battery_ = 700 mWh → batteryDrawPotential = 300 W
   - budgetThisMinute_ = 0 + 300 = 300 W
   - consumePower(300) → battery_ = 700–300 = 400 mWh, Available = 0 W, deposition ⟶ 47/60 ✅

 Minute 47 (eclipse):
   - producedThisMinute_ = 0 W
   - battery_ = 400 mWh → batteryDrawPotential = 300 W
   - budgetThisMinute_ = 0 + 300 = 300 W
   - consumePower(300) → battery_ = 400–300 = 100 mWh, Available = 0 W, deposition ⟶ 48/60 ✅

 Minute 48 (eclipse):
   - producedThisMinute_ = 0 W
   - battery_ = 100 mWh → batteryDrawPotential = 100 W
   - budgetThisMinute_ = 0 + 100 = 100 W
   - Now 100 W < 300 W required → cannot consumePower(300)
   - deposition stalls (“Waiting”), since battery-only budget is too small

 In short:
   • You get three full minutes of 300 W battery draw in eclipse (battery: 1000→700→400→100), 
     so deposition can run from minute 45 through 47.
   • On the fourth eclipse minute, only 100 W remains in battery → budgetThisMinute_ = 100 W,
     which is insufficient for the 300 W deposition load, so the task waits.
*/


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



