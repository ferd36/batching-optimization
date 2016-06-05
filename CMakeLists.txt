cmake_minimum_required(VERSION 3.5)
project(batching_experiments_results)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES batch_optimization.cpp)

add_executable(batching_experiments_results ${SOURCE_FILES})