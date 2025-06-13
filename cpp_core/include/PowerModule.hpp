#ifndef POWER_MODULE_HPP        // ---------- Include guard: avoids double inclusion
#define POWER_MODULE_HPP

#include <string>

/**
 * ESSENTIALLY A BLUEPRINT, functions are not implemented here.
 * @brief  Tracks solar-panel generation, battery state, and power consumption.
 *
 * Usage pattern in main loop:
 *     power.update(t, orbitalPhase);          // refresh available power
 *     if (power.canSatisfyDemand(neededW)) {
 *         power.consumePower(neededW);        // deduct watts from budget
 *     }
 */
class PowerModule {
public:
    /* ---------- constructor ---------- */
    PowerModule(int maxBattery   = 250'000,   // mWh
                int genSunlight  =   300,    // W produced per minute in sunlight
                int genEclipse   =     0);   // W produced per minute in eclipse

    /* ---------- per-timestep API ---------- */
    void update(int t, const std::string& orbitalPhase);
        // - Recharges / discharges battery based on orbit phase
        // - Resets the “budget” for this minute

    bool canSatisfyDemand(int watts) const;
        // Returns true if watts ≤ surplus for current minute

    void consumePower(int watts);
        // Deducts watts from available budget and draws from battery as needed

    /* ---------- getters for other modules / logger ---------- */
    int  getAvailablePower() const;   // Remaining budget this minute (W)
    int  getBatteryLevel()   const;   // Battery state of charge (mWh)
    int  getLastProduced()   const;   // Solar generation this minute (W)

private:
    /* ---------- persistent state ---------- */
    int battery_;            // current state of charge (mWh)
    int maxBattery_;         // capacity (mWh)

    /* generation rates */
    int genSunlight_;        // W in full sunlight
    int genEclipse_;         // W in eclipse

    /* ---------- per-minute scratch ---------- */
    int producedThisMinute_; // actual solar W produced this minute
    int budgetThisMinute_;   // reset by update()

    /* helper */
    int solarGeneration(const std::string& phase) const;  // W for current phase
};

#endif  // POWER_MODULE_HPP
