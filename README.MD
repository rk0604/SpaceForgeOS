# 🚀 SpaceForgeOS

**SpaceForgeOS** is a C++ and Deep Learning-powered simulation platform for exploring the future of **orbital manufacturing**. Designed to simulate, optimize, and visualize the production of components in space environments, it combines systems engineering with artificial intelligence to tackle challenges unique to microgravity fabrication.

---

## 🧠 Key Module: OrbFabSim

**OrbFabSim** is the primary simulator within SpaceForgeOS. It models a set of manufacturing modules ("Teemori Modules") onboard an orbital platform and uses deep learning to schedule tasks and manage power, time, and resource constraints.

### ✨ Features
- Modular simulation of space-based manufacturing processes
- Deep learning-based scheduling and optimization
- Optional orbital phase simulation (e.g. sunlight windows, communication access)
- Expandable support for power, thermal, and material constraints

---

## 🛠 Tech Stack

| Component              | Tech                                  |
|------------------------|---------------------------------------|
| Core Simulation        | C++                                   |
| Deep Learning          | PyTorch (or optionally TorchScript)   |
| Optimization Logic     | Reinforcement Learning / Scheduling   |
| Visualization / UI     | Qt (desktop) or Web (React + Flask)   |
| Orbital Physics (Optional) | Orekit or Custom Physics Engine (C++) |

---

## 📁 Project Structure
SpaceForgeOS/
├── simulator/ # Main C++ simulator
├── scheduler_dl/ # Deep learning scheduler (PyTorch)
├── orbital_dynamics/ # Orbital physics engine (optional)
├── docs/ # Diagrams and architecture
├── ui/ # Optional frontend (Qt or web-based)
└── scripts/ # CLI and automation scripts


---

## 🛰 Project Goals

- Simulate orbital manufacturing environments with realistic constraints.
- Use AI to dynamically allocate power and time across tasks.
- Provide a framework to test future orbital fabrication strategies.

---

## 📌 Future Plans

- Integrate live orbital ephemeris data
- Add support for thermal and vacuum environment models
- Export manufacturing performance metrics for analysis

---

## 🧑‍💻 Author

**Rishab Kokate**  
CS & Data Science @ Rutgers • Aerospace Enthusiast • [LinkedIn](https://www.linkedin.com/in/rishab-kokate-58a81722b/)  
Contact: `rishabk2004@gmail.com`

---

> “Space isn’t remote at all. It’s only an hour’s drive away — if your car could go straight upwards.” – *Fred Hoyle*s