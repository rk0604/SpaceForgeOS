//Execute queued tasks, track duration and state
// make factoryTask struct that defines task specs (name, duration, and power usage);


// this file is supposed to be the factory module class 
#include "DepositionModule.hpp"
#include <algorithm>  

DepositionModule::DepositionModule() {
    elapsed = 0;
    activeTask = nullptr;
}
