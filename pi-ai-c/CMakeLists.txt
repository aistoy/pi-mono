cmake_minimum_required(VERSION 3.10)
project(pi-ai-c C)

set(CMAKE_C_STANDARD 99)

# Third party: cJSON
add_subdirectory(third_party/cJSON)

# Include directories
include_directories(include)
include_directories(third_party/cJSON)

# Try to find Curl
find_package(CURL)

if(NOT CURL_FOUND)
    message(WARNING "CURL not found via find_package, attempting manual configuration")
    # In some environments we might need to manually specify
    set(CURL_LIBRARIES "curl")
endif()

# Library
file(GLOB_RECURSE LIB_SOURCES src/*.c)
add_library(pi_ai_c SHARED ${LIB_SOURCES})
set_target_properties(pi_ai_c PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(pi_ai_c cjson ${CURL_LIBRARIES})

# Example
add_executable(pi_ai_example examples/main.c)
target_link_libraries(pi_ai_example pi_ai_c)

# Installation (optional)
install(TARGETS pi_ai_c DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
