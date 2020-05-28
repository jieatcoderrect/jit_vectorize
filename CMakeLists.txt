cmake_minimum_required(VERSION 3.13)
project(project)

set(CMAKE_CXX_STANDARD 17)


#if(${CMAKE_BUILD_TYPE} MATCHES "Release")
#    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -mavx2")
#endif()

message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}s")
message(STATUS "CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")

add_executable(project main.cpp)
add_executable(disjunctive main_disjunctive.cpp)
add_executable(conjunctive main_conjunctive.cpp common.h)
add_executable(synthesis main_synthesis.cpp common.h)

add_executable(simpleinterp bfjit/simpleinterp.cpp)
add_executable(simplevm bfjit/simplevm.cpp)
