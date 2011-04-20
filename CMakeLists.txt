# Project name is not mandatory, but you should use it
project(pebil C CXX Fortran)

# States that CMake required version must be >= 2.6
cmake_minimum_required(VERSION 2.6)

set(PEBIL_FULL_VER "1.1.3-devel")

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-w -g")
set(CMAKE_C_FLAGS_RELEASE "-w -g")

set(CMAKE_INSTALL_PREFIX ".")

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(testapps)
add_subdirectory(instcode)

enable_testing()
add_test(NAME pebilide-dynTest
  COMMAND pebil --typ ide --app testapps/dynTest)
add_test(NAME pebilide-cTest
  COMMAND pebil --typ ide --app testapps/cTest)
#add_test(NAME pebilide-cppTest
#  COMMAND pebil --typ ide --app testapps/cppTest)
add_test(NAME pebilide-htcoef
  COMMAND pebil --typ ide --app testapps/htcoef)

