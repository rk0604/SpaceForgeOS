cmake_minimum_required(VERSION 3.10)
project(SpaceForgeCore)

set(CMAKE_CXX_STANDARD 17)

# Add source files
file(GLOB SOURCES "src/*.cpp")

# Include headers
include_directories(include)

# Create the executable or library
add_executable(spaceforge_engine ${SOURCES})
include_directories(${CMAKE_SOURCE_DIR}/include)
