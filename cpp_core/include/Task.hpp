#ifndef TASK_HPP
#define TASK_HPP

#include <string>
#include <array>
#include <algorithm>   // std::max

/**
 * @brief  Full life-cycle record for a single wafer (“task”).
 *
 *  ─────────── Manufacturing Stages ───────────
 *   0. Deposition        – nominal 60 min
 *   1. Ion Implantation  – nominal 20 min
 *   2. Crystal Growth    – nominal 120 min
 * 
 *  Each task is exactly one wafer job 
 *  Each Task owns an array of PhaseInfo, one entry per stage, so that:
 *    • Every module touches only its PhaseInfo (single-writer rule)
 *    • The logger or ONNX inference can read global wafer status in O(1)
 */
struct Task {

    /* ----- Per-phase bookkeeping ----- */
    struct PhaseInfo {
        int requiredTime = 0;   // minutes needed for this phase
        int elapsedTime  = 0;   // minutes processed so far
        int energyUsed   = 0;   // cumulative watt-minutes USED 
        bool wasInterrupted = false;   // true if the phase was paused/stalled mid-run
        double defectChance = 0.0;     // the error rate for this phase (e.g., 0.01 = 1%)
        bool defective = false;        // whether this phase had a defect

        bool   isDone()        const { return elapsedTime >= requiredTime; }
        int    timeRemaining() const { return std::max(0, requiredTime - elapsedTime); }
    };

    /* ----- Persistent wafer identity ----- */
    std::string id;                     // e.g. "T_3"

    /* ----- Three manufacturing stages ----- */
    std::array<PhaseInfo, 3> phase;     // [0]=Depo, [1]=Ion, [2]=Crystal

    /* ----- Pointer to current stage ----- */
    int currentStage = 0;               // 0..2; 3 ⇒ wafer finished


    /* ----- Convenience helpers ----- */
    bool isComplete()            const { return currentStage >= 3; }

    PhaseInfo&       currentPhase()       { return phase[currentStage]; }
    const PhaseInfo& currentPhase() const { return phase[currentStage]; }

    int totalEnergy() const {
        int sum = 0;
        for (const auto& p : phase) sum += p.energyUsed;
        return sum;
    }

    // check if the current phase has failed
    bool phaseFail() const {    return currentPhase().defective;}
    
};

#endif  // TASK_HPP
