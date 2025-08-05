cmake_minimum_required(VERSION 3.15)
project(SpaceForgeSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: Project Chrono hook
option(USE_CHRONO "Enable Project Chrono support" OFF)
if (USE_CHRONO)
    find_package(Chrono REQUIRED)
endif()

include_directories(include)
file(GLOB SRC_FILES src/*.cpp)

add_executable(sim ${SRC_FILES})
if (USE_CHRONO)
    target_link_libraries(sim ChronoEngine)
endif()