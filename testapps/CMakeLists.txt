# Project name is not mandatory, but you should use it
project(hello_world C CXX Fortran)

# States that CMake required version must be >= 2.6
cmake_minimum_required(VERSION 2.6)

#find_package(MPI)
#find_program(MPI_C_COMPILER NAMES mpicc mpxlc mpxlc_r mpcc cc)
#if(NOT MPI_C_COMPILER)
#  message(STATUS "Could not find MPI C Compiler, falling back to serial")
#else(MPI_C_COMPILER)
#  set(CMAKE_C_COMPILER ${MPI_C_COMPILER})
#  add_definitions(-DHAVE_MPI)
#  message(STATUS "Found MPI C Compiler for instcode. Using -DHAVE_MPI")
#endif(NOT MPI_C_COMPILER)

link_libraries("-lm")
include_directories("../instcode")

# Create a variable called helloworld_SOURCES containing all .cpp files:
set(dynTest_SOURCES dynTest.c)
set(cTest_SOURCES foo.c bar.c dum.c main.c)
#set(cppTest_SOURCES foo.c bar.c dum.c main.c classes.C)
set(htcoef_SOURCES htcoef.f)
set(sgbench_SOURCES sgbench.c)
## NOTE: this is now built out of instcode/ so we that don't use the MPI compiler here
#set(ioTest_SOURCES ioTest.c)

# Create an executable file called helloworld from sources:
add_executable(dynTest ${dynTest_SOURCES})
add_executable(cTest ${cTest_SOURCES})
#add_executable(cppTest ${cppTest_SOURCES})
add_executable(htcoef ${htcoef_SOURCES})
add_executable(sgbench ${sgbench_SOURCES})

add_test(NAME pebilide-dynTest
  COMMAND pebil --typ ide --app dynTest)
add_test(NAME pebilide-cTest
  COMMAND pebil --typ ide --app cTest)
#add_test(NAME pebilide-cppTest                                                                                                                                        
#  COMMAND pebil --typ ide --app testapps/cppTest) 
add_test(NAME pebilide-htcoef
  COMMAND pebil --typ ide --app htcoef)
