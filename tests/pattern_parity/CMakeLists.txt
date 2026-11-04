cmake_minimum_required(VERSION 3.15)
project(pattern_parity CXX)

# Standalone test: builds the plugin's pattern generator without JUCE/Open Ephys.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(parity_runner
    parity_runner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/PatternGen/PatternGenerator.cpp)
