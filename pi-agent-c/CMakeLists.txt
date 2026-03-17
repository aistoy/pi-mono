cmake_minimum_required(VERSION 3.10)
project(pi-agent-c C)

set(CMAKE_C_STANDARD 99)

# Paths to pi-ai-c
set(PI_AI_C_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../pi-ai-c")
set(PI_AI_C_INCLUDE "${PI_AI_C_DIR}/include")
set(PI_AI_C_LIB_DIR "${PI_AI_C_DIR}/build") # Assuming built there

include_directories(include)
include_directories(${PI_AI_C_INCLUDE})
include_directories(${PI_AI_C_DIR}/third_party/cJSON)

# Find pi_ai_c library
find_library(PI_AI_C_LIB NAMES pi_ai_c HINTS ${PI_AI_C_LIB_DIR})

# Library
file(GLOB_RECURSE LIB_SOURCES src/*.c)
add_library(pi_agent_c SHARED ${LIB_SOURCES})
target_link_libraries(pi_agent_c ${PI_AI_C_LIB})

# Example
add_executable(agent_example examples/calc_agent.c)
target_link_libraries(agent_example pi_agent_c)
