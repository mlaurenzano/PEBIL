project(pebil_tools CXX)
cmake_minimum_required(VERSION 2.6)
if (POLICY CMP0015)
   cmake_policy(SET CMP0015 NEW)
endif (POLICY CMP0015)

add_definitions(-DPEBIL_VER="${PEBIL_FULL_VER}")

# locate mpi
find_package(MPI)
if(NOT MPI_FOUND)
  message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}: could not find MPI; falling back to serial configuration")
else(MPI_FOUND)
  add_definitions(-DHAVE_MPI)
  message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}: found MPI; using parallel configuration (-DHAVE_MPI)")
endif(NOT MPI_FOUND)

# For a large number of source files you can create it in a simpler way
file(GLOB pebil_SOURCES *.C)
include_directories("." "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/external/udis86-1.7" "${CMAKE_SOURCE_DIR}/instcode")
link_directories("${CMAKE_SOURCE_DIR}/src/" "${CMAKE_SOURCE_DIR}/external/udis86-1.7/libudis86/.libs")

add_executable(pebil ${pebil_SOURCES})
target_link_libraries(pebil "libpebilinst.so" "libudis86.a")

install(
  TARGETS pebil
  RUNTIME DESTINATION "${CMAKE_BINARY_DIR}/bin"
)