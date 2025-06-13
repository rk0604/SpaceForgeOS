#ifndef ORBIT_MODEL_HPP
#define ORBIT_MODEL_HPP

#include <string>

class OrbitModel {
public:
    std::string getPhase(int t) const {
        return (t % 90 < 45) ? "sunlight" : "eclipse";
    }
};

#endif
