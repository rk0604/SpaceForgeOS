cmake_minimum_required(VERSION 3.20)
project(SpaceForgeCore LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Paths ----
set(PROJ_SRC_DIR        ${CMAKE_SOURCE_DIR}/src)
set(PROJ_INC_DIR        ${CMAKE_SOURCE_DIR}/include)
set(SPARTA_SRC_DIR      ${CMAKE_SOURCE_DIR}/external/sparta/src)   # contains library.h and libsparta_mpi.a
set(SPARTA_STATIC_LIB   ${SPARTA_SRC_DIR}/libsparta_mpi.a)         # adjust if you built serial: libsparta.a

# ---- MPI ----
find_package(MPI REQUIRED)

# ---- Sources ----
file(GLOB SRC_FILES ${PROJ_SRC_DIR}/*.cpp)

# ---- Executable ----
add_executable(simulation ${SRC_FILES})

target_include_directories(simulation PRIVATE
  ${PROJ_INC_DIR}
  ${SPARTA_SRC_DIR}
)

# Import the prebuilt SPARTA static library
add_library(sparta_mpi STATIC IMPORTED GLOBAL)
set_target_properties(sparta_mpi PROPERTIES
  IMPORTED_LOCATION ${SPARTA_STATIC_LIB}
)

# Link
target_link_libraries(simulation PRIVATE sparta_mpi MPI::MPI_CXX)

# (Optional) stricter warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(simulation PRIVATE -Wall -Wextra -Wpedantic)
endif()
