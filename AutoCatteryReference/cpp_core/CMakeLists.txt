cmake_minimum_required(VERSION 3.20)
project(autocattery_core VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(autocattery_core
    src/scoring.cpp
    src/classification.cpp
    src/room_planner.cpp
)
target_include_directories(autocattery_core PUBLIC include)
if(MSVC)
  target_compile_options(autocattery_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(autocattery_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(autocattery_core_tests tests/test_main.cpp)
target_link_libraries(autocattery_core_tests PRIVATE autocattery_core)
enable_testing()
add_test(NAME autocattery_core_tests COMMAND autocattery_core_tests)
