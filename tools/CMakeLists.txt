project(pebil_tools CXX)
cmake_minimum_required(VERSION 2.6)

add_definitions(-DPEBIL_VER="${PEBIL_FULL_VER}")

# locate mpi compilers
find_package(MPI)
find_program(MPI_C_COMPILER NAMES mpicc mpxlc mpxlc_r mpcc cc)
if(NOT MPI_C_COMPILER)
  message(STATUS "Could not find MPI C Compiler, falling back to serial")
else(MPI_C_COMPILER)
  add_definitions(-DHAVE_MPI)
  message(STATUS "Found MPI C Compiler for tools. Using -DHAVE_MPI")
endif(NOT MPI_C_COMPILER)

# For a large number of source files you can create it in a simpler way
file(GLOB pebil_SOURCES *.C)
include_directories("." "../include" "../external/udis86-1.7" "../instcode")
link_directories("../src/" "../external/udis86-1.7/libudis86/.libs")

add_executable(pebil ${pebil_SOURCES})
target_link_libraries(pebil "libpebilinst.so" "libudis86.a")

install(
  TARGETS pebil
  RUNTIME DESTINATION "bin"
)