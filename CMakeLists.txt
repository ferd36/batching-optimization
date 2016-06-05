cmake_minimum_required(VERSION 2.8)
project(batching_experiment)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -DNDEBUG -O3")

set(SOURCE_FILES batch_optimization.cpp)

add_executable(batching ${SOURCE_FILES})