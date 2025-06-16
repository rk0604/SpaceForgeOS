#include "IonImplantationModule.hpp"
#include <iostream>
#include <algorithm>
#include <fstream> 
#include <cstdlib> // For rand(), srand()
#include <ctime>   // For time()

// constructor
IonImplantationModule:: IonImplantationModule():
    activeTask(nullptr), elapsed(0) // initialization list
    {   
        std::cout << "Called: IonImplantationModule::IonImplantationModule()" << std::endl;
    }