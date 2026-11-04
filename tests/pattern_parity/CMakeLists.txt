cmake_minimum_required(VERSION 3.15)
project(pattern_parity CXX)

# Standalone tests: build the plugin's pattern generator and model without JUCE/Open Ephys.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PATTERN_GEN ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/PatternGen)

add_executable(parity_runner
    parity_runner.cpp
    ${PATTERN_GEN}/PatternGenerator.cpp)

add_executable(model_tests
    model_tests.cpp
    ${PATTERN_GEN}/PatternGenerator.cpp
    ${PATTERN_GEN}/PatternModel.cpp
    ${PATTERN_GEN}/Formula.cpp
    ${PATTERN_GEN}/Json.cpp)
