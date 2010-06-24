# Project name is not mandatory, but you should use it
project(pebil_instcode C)

# States that CMake required version must be >= 2.6
cmake_minimum_required(VERSION 2.6)

# locate mpi compilers
find_package(MPI)
find_program(MPI_C_COMPILER NAMES mpicc mpxlc mpxlc_r mpcc)
if(NOT MPI_C_COMPILER)
  message(STATUS "Could not find MPI C Compiler, falling back to serial")
else(MPI_C_COMPILER)
  set(CMAKE_C_COMPILER ${MPI_C_COMPILER})
  set(USING_MPI_WRAPPERS 1)
  message(STATUS "Found MPI C Compiler! Defining USING_MPI_WRAPPERS")
endif(NOT MPI_C_COMPILER)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-DNO_SAMPLING_MODE -DPER_SET_RECENT")
#set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-DEXTENDED_SAMPLING -DPER_SET_RECENT")
#set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-DNO_SAMPLING_MODE -DPER_SET_RECENT -DIGNORE_ACTUAL_SIMULATION")

# Create a variable called helloworld_SOURCES containing all .cpp files:
set(counter_SOURCES CounterFunctions.c)
set(simulator_SOURCES SimulationFunctions.c)
set(timer_SOURCES TimerFunctions.c)
set(tracer_SOURCES TracerFunctions.c)

# set this globally? doesn't make sense here
include_directories(".")

# set link/include vars
#target_link_libraries(helloc mpi)
#set(helloc_INCLUDES ${MPI_INCLUDE_PATH})
add_library(counter SHARED ${counter_SOURCES})
add_library(simulator SHARED ${simulator_SOURCES})
add_library(timer SHARED ${timer_SOURCES})
add_library(tracer SHARED ${tracer_SOURCES})

install(TARGETS counter DESTINATION "lib")
install(TARGETS simulator DESTINATION "lib")
install(TARGETS timer DESTINATION "lib")
install(TARGETS tracer DESTINATION "lib")
