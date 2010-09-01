# Project name is not mandatory, but you should use it
project(pebil_instcode C)

# States that CMake required version must be >= 2.6
cmake_minimum_required(VERSION 2.6)

# locate mpi compilers
find_package(MPI)
find_program(MPI_C_COMPILER NAMES mpicc mpxlc mpxlc_r mpcc cc)
if(NOT MPI_C_COMPILER)
  message(STATUS "Could not find MPI C Compiler, falling back to serial")
else(MPI_C_COMPILER)
  set(CMAKE_C_COMPILER ${MPI_C_COMPILER})
  add_definitions(-DHAVE_MPI)
  message(STATUS "Found MPI C Compiler for instcode. Using -DHAVE_MPI")
endif(NOT MPI_C_COMPILER)

add_definitions(-DPER_SET_RECENT)
add_definitions(-DSHIFT_ADDRESS_BUFFER)

# pick the sampling mode
#add_definitions(-DNO_SAMPLING_MODE)
add_definitions(-DEXTENDED_SAMPLING_MODE)
#add_definitions(-DNO_SAMPLING_MODE -DIGNORE_ACTUAL_SIMULATION)

ADD_CUSTOM_COMMAND(
        OUTPUT IOEvents.C IOEvents.h
        COMMAND ../scripts/generateIOevents.py
        DEPENDS ioevents.xml
)
ADD_CUSTOM_TARGET(ioevents ALL DEPENDS IOEvents.h)

# Create a variable called helloworld_SOURCES containing all .cpp files:
set(counter_SOURCES CounterFunctions.c)
set(simulator_SOURCES SimulationFunctions.c CacheSimulationCommon.c)
set(timer_SOURCES TimerFunctions.c)
set(iowrapper_SOURCES IOWrappers.c)
set(iobin2txt_SOURCES iobin2txt.c)

# set this globally? doesn't make sense here
include_directories(".")

# set link/include vars
#target_link_libraries(helloc mpi)
#set(helloc_INCLUDES ${MPI_INCLUDE_PATH})
add_library(counter SHARED ${counter_SOURCES})
add_library(simulator SHARED ${simulator_SOURCES})
add_library(timer SHARED ${timer_SOURCES})
add_library(iowrapper SHARED ${iowrapper_SOURCES})
add_executable(iobin2txt ${iobin2txt_SOURCES})

install(TARGETS counter DESTINATION "lib")
install(TARGETS simulator DESTINATION "lib")
install(TARGETS timer DESTINATION "lib")
install(TARGETS iowrapper DESTINATION "lib")
install(TARGETS iobin2txt DESTINATION "bin")