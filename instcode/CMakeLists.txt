# Project name is not mandatory, but you should use it
project(pebil_instcode C)

# States that CMake required version must be >= 2.6
cmake_minimum_required(VERSION 2.6)
if (POLICY CMP0015)
   cmake_policy(SET CMP0015 NEW)
endif (POLICY CMP0015)

# locate mpi variables
find_package(MPI)
if(NOT MPI_FOUND)
  message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}: could not find MPI; falling back to serial configuration")
else(MPI_FOUND)
  add_definitions(-DHAVE_MPI)
  message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}: found MPI; using parallel configuration (-DHAVE_MPI)")
endif(NOT MPI_FOUND)

add_definitions(-DPER_SET_RECENT)
add_definitions(-DSHIFT_ADDRESS_BUFFER)

# pick the sampling mode
#add_definitions(-DNO_SAMPLING_MODE)
add_definitions(-DEXTENDED_SAMPLING_MODE)
#add_definitions(-DNO_SAMPLING_MODE -DIGNORE_ACTUAL_SIMULATION)

# Create a variable called helloworld_SOURCES containing all .cpp files:
set(counter_SOURCES CounterFunctions.c)
set(simulator_SOURCES SimulationFunctions.c CacheSimulationCommon.c)
set(timer_SOURCES TimerFunctions.c)

# set this globally? doesn't make sense here
include_directories(".")

# set target/link
add_library(counter SHARED ${counter_SOURCES})
add_library(simulator SHARED ${simulator_SOURCES})
add_library(timer SHARED ${timer_SOURCES})

install(TARGETS counter simulator timer DESTINATION "${CMAKE_BINARY_DIR}/lib")
