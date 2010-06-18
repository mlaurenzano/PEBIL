# Project name is not mandatory, but you should use it
project(hello_world C CXX Fortran)

# States that CMake required version must be >= 2.6
cmake_minimum_required(VERSION 2.6)

# Create a variable called helloworld_SOURCES containing all .cpp files:
set(dynTest_SOURCES dynTest.c)
set(cTest_SOURCES foo.c bar.c dum.c main.c)
#set(cppTest_SOURCES foo.c bar.c dum.c main.c classes.C)
set(htcoef_SOURCES htcoef.f)

# set this globally? doesn't make sense here
set(CMAKE_C_COMPILER ${MPI_C_COMPILER})
set(CMAKE_Fortran_COMPILER ${MPI_F_COMPILER})

# For a large number of source files you can create it in a simpler way
# using file() function:
# file(GLOB hellworld_SOURCES *.cpp)

# Create an executable file called helloworld from sources:
add_executable(dynTest ${dynTest_SOURCES})
add_executable(cTest ${cTest_SOURCES})
#add_executable(cppTest ${cppTest_SOURCES})
add_executable(htcoef ${htcoef_SOURCES})

# set compiler for a single target -- doesn't seem to work
#set_target_properties(helloc PROPERTIES CMAKE_C_COMPILER ${MPI_C_COMPILER})

# set link/include vars
#target_link_libraries(helloc mpi)
#set(helloc_INCLUDES ${MPI_INCLUDE_PATH})