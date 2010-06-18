# Project name is not mandatory, but you should use it
project(hello_world C CXX Fortran)

# States that CMake required version must be >= 2.6
cmake_minimum_required(VERSION 2.6)

# Create a variable called helloworld_SOURCES containing all .cpp files:
set(dynTest_SOURCES dynTest.c)
set(cTest_SOURCES foo.c bar.c dum.c main.c)
#set(cppTest_SOURCES foo.c bar.c dum.c main.c classes.C)
set(htcoef_SOURCES htcoef.f)

# Create an executable file called helloworld from sources:
add_executable(dynTest ${dynTest_SOURCES})
add_executable(cTest ${cTest_SOURCES})
#add_executable(cppTest ${cppTest_SOURCES})
add_executable(htcoef ${htcoef_SOURCES})
