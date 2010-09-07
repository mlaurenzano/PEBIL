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

ADD_CUSTOM_COMMAND(
        OUTPUT IOEvents.C IOEvents.h
        COMMAND ../scripts/iogen.py
        DEPENDS ioevents.xml
)
ADD_CUSTOM_TARGET(ioevents ALL DEPENDS IOEvents.h)

# Create a variable called helloworld_SOURCES containing all .cpp files:
set(counter_SOURCES CounterFunctions.c)
set(simulator_SOURCES SimulationFunctions.c CacheSimulationCommon.c)
set(timer_SOURCES TimerFunctions.c)
set(iowrapper_SOURCES IOWrappers.c)
set(iobin2txt_SOURCES iobin2txt.c IOEvents.h)
set(ioTest_SOURCES ${CMAKE_SOURCE_DIR}/testapps/ioTest.c)

# set this globally? doesn't make sense here
include_directories(".")

# set target/link
add_library(counter SHARED ${counter_SOURCES})
add_library(simulator SHARED ${simulator_SOURCES})
add_library(timer SHARED ${timer_SOURCES})
add_library(iowrapper SHARED ${iowrapper_SOURCES})
add_library(iowrapper_preld SHARED ${iowrapper_SOURCES})
add_executable(iobin2txt ${iobin2txt_SOURCES})
add_executable(ioTest ${ioTest_SOURCES})

set_target_properties(iowrapper_preld PROPERTIES COMPILE_FLAGS "-DPRELOAD_WRAPPERS")

# figure out whether to use an mpi compiler driver or not
if (MPI_FOUND)
  find_program(MPI_C_COMPILER NAMES mpicc mpxlc mpxlc_r mpcc)
  if (MPI_C_COMPILER)
    set(CMAKE_C_COMPILER ${MPI_C_COMPILER})
    message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}: using MPI_C_COMPILER=`${MPI_C_COMPILER}`")
  else (NOT MPI_C_COMPILER)
    include_directories(${MPI_INCLUDE})
    target_link_libraries(iobin2txt ${MPI_LIBRARY})
    target_link_libraries(ioTest ${MPI_LIBRARY})
    message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}: no MPI C compiler found, using MPI_LIBRARY=`${MPI_LIBRARY}` and MPI_INCLUDE=`${MPI_INCLUDE}`")
  endif(MPI_C_COMPILER)
endif(MPI_FOUND)

install(TARGETS counter simulator timer iowrapper iowrapper_preld DESTINATION "${CMAKE_BINARY_DIR}/lib")
install(TARGETS iobin2txt DESTINATION "${CMAKE_BINARY_DIR}/bin")
install(TARGETS ioTest DESTINATION "${CMAKE_BINARY_DIR}/testapps")
