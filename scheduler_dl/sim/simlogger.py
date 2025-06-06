import pandas as pd, random
from collections import deque

def simulationRun():
    # ---------- CONFIG ----------
    task_ids  = [f"T_{i}" for i in range(10)]
    phases    = ['deposition', 'ion_imp', 'cry_grw']
    phase_len = {'deposition': 60, 'ion_imp': 20, 'cry_grw': 120}

    MAX_MIN      = 1440        # 24 h
    LOG_INT      = 10          # log every 10 min
    ORBIT_PERIOD = 90          # 45 min sun / 45 min eclipse

    # ---------- pipeline state ----------
    phase_queue  = {ph: deque(task_ids if ph == 'deposition' else []) for ph in phases}
    active_task  = {ph: None for ph in phases}
    prog = {tid: {ph: dict(elapsed=0, energy=0, status='waiting')
                  for ph in phases} for tid in task_ids}

    throughput = 0
    log = []

    # ---------- MAIN LOOP ----------
    for t in range(MAX_MIN):
        orb_phase = 'sunlight' if (t % ORBIT_PERIOD) < ORBIT_PERIOD//2 else 'eclipse'

        for ph in phases:
            # pull next wafer if station idle
            if active_task[ph] is None and phase_queue[ph]:
                tid = phase_queue[ph].popleft()
                active_task[ph] = tid
                prog[tid][ph]['status'] = 'processing'

            tid = active_task[ph]

            if tid is not None:                              # process 1 min
                rec = prog[tid][ph]
                demand  = random.randint(30, 80)
                used    = demand if random.random() > 0.1 else random.randint(0, demand)
                rec['energy']  += used
                rec['elapsed'] += 1

                if rec['elapsed'] >= phase_len[ph]:          # phase finished
                    rec['status'] = 'done'
                    active_task[ph] = None
                    nxt = phases.index(ph) + 1
                    if nxt < len(phases):
                        phase_queue[phases[nxt]].append(tid)
                    else:
                        throughput += 1                      # wafer complete
            # ----------------------------------------------

        # ------------- LOG (exactly 30 rows) --------------
        if t % LOG_INT == 0:
            for tid in task_ids:
                for ph in phases:
                    rec = prog[tid][ph]
                    # power numbers only for the phase currently processing this wafer
                    power_used = power_demand = 0
                    if active_task[ph] == tid and rec['status'] == 'processing':
                        power_demand = random.randint(30, 80)
                        power_used   = power_demand
                        rec['energy'] += power_used          # account in cum energy

                    log.append({
                        'timestamp'                     : t,
                        'task_id'                       : tid,
                        'task_phase_id'                 : f"{tid}_{ph}",
                        'task_phase_name'               : ph,
                        'phase_time_remaining(min)'     : max(phase_len[ph] - rec['elapsed'], 0),
                        'phase_power_demand(w)'         : power_demand,
                        'phase_power_used(w)'           : power_used,
                        'phase_energy_consumed_cum(w)'  : rec['energy'],
                        'battery_level'                 : random.randint(30, 100),
                        'task_phase_status'             : rec['status'],
                        'failure_event'                 : 0,
                        'orbital_phase'                 : orb_phase,
                        'task_throughput_counter'       : throughput
                    })

    df = pd.DataFrame(log)
    df.to_csv('sample_sim_log.csv', index=False)
    return df


if __name__ == "__main__":
    df = simulationRun()
    # quick check: every timestamp must have 30 rows
    print(df.groupby('timestamp').size().head())
    print("rows, cols:", df.shape)

'''
Simulation Overview: Orbital Semiconductor Manufacturing Pipeline
------------------------------------------------------------------

This simulation models 10 wafers flowing through 3 sequential manufacturing phases:
  1. Deposition        → 60 minutes
  2. Ion Implantation  → 20 minutes
  3. Crystal Growth    → 120 minutes

Processing Model:
------------------
- Only one wafer is processed at a time per phase (1 station per phase).
- Phases operate in a pipeline fashion: as soon as one wafer finishes a phase,
  it moves to the next phase's queue, and the next wafer can begin processing.
- Wafers are processed in order (T_0 to T_9).

Important Timing Milestones:
-----------------------------
- At time t =   0: T_0 starts Deposition.
- At time t =  60: T_0 starts Ion Implantation, T_1 starts Deposition.
- At time t =  80: T_0 starts Crystal Growth, T_1 starts Ion Implantation, T_2 starts Deposition.
- At time t = 200: T_0 completes all 3 phases (Depo + Ion + Crystal Growth).
- From t = 320 onward: A new wafer finishes every 120 minutes (Crystal Growth bottleneck).

Completion Schedule:
---------------------
- T_0:  finishes at t = 200
- T_1:  finishes at t = 320
- T_2:  finishes at t = 440
- ...
- T_9:  finishes at t = 1280

Key Insights:
--------------
- The pipeline becomes “full” by t = 80, after which all three stations are continuously active.
- The simulation logs every 10 minutes: each log contains 30 rows (3 phases x 10 wafers).
- Crystal Growth is the rate-limiting step (longest duration), which defines the system throughput.
- The simulation ends at t = 1440 (24 hours), with all 10 wafers processed.

'''
