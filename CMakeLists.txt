cmake_minimum_required(VERSION 3.10)
project(beamline_rt_sim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# Enable compile_commands.json for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(Threads REQUIRED)

# Find ZeroMQ using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZeroMQ REQUIRED libzmq)

message(STATUS "Found ZeroMQ: ${ZeroMQ_LIBRARIES}")
message(STATUS "ZeroMQ include dirs: ${ZeroMQ_INCLUDE_DIRS}")
message(STATUS "ZeroMQ library dirs: ${ZeroMQ_LIBRARY_DIRS}")
message(STATUS "ZeroMQ link flags: ${ZeroMQ_LDFLAGS}")

# Add include directories
include_directories(${ZeroMQ_INCLUDE_DIRS})

# Create third_party directory for header-only libraries
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann)

# Download nlohmann/json header if not present
set(JSON_HEADER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/json.hpp)
if(NOT EXISTS ${JSON_HEADER_PATH})
    message(STATUS "Downloading nlohmann/json header...")
    file(DOWNLOAD 
        "https://github.com/nlohmann/json/releases/download/v3.10.5/json.hpp"
        ${JSON_HEADER_PATH}
        SHOW_PROGRESS
    )
endif()

# Add include directory for nlohmann/json
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# Main executable (will be populated as we implement components)
add_executable(beamline_rt_sim
    src/main.cpp
)

target_link_libraries(beamline_rt_sim PRIVATE 
    ${ZeroMQ_LDFLAGS} 
    Threads::Threads
)

target_include_directories(beamline_rt_sim PRIVATE src)

# Enable testing
enable_testing()

# Test executables
add_executable(test_timing tests/test_timing.cpp)
target_link_libraries(test_timing PRIVATE Threads::Threads)
target_include_directories(test_timing PRIVATE src)
add_test(NAME timing COMMAND test_timing)

add_executable(test_ring_buffer tests/test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE Threads::Threads)
target_include_directories(test_ring_buffer PRIVATE src)
add_test(NAME ring_buffer COMMAND test_ring_buffer)

add_executable(test_pid tests/test_pid.cpp)
target_link_libraries(test_pid PRIVATE Threads::Threads)
target_include_directories(test_pid PRIVATE src)
add_test(NAME pid COMMAND test_pid)

add_executable(test_telemetry tests/test_telemetry.cpp)
target_link_libraries(test_telemetry PRIVATE Threads::Threads)
target_include_directories(test_telemetry PRIVATE src)
add_test(NAME telemetry COMMAND test_telemetry)

# Comprehensive Stress Tests
add_executable(stress_test_clock tests/stress_test_clock.cpp)
target_link_libraries(stress_test_clock PRIVATE Threads::Threads)
target_include_directories(stress_test_clock PRIVATE src)
add_test(NAME stress_clock COMMAND stress_test_clock)

add_executable(stress_test_ring_buffer tests/stress_test_ring_buffer.cpp)
target_link_libraries(stress_test_ring_buffer PRIVATE Threads::Threads)
target_include_directories(stress_test_ring_buffer PRIVATE src)
add_test(NAME stress_ring_buffer COMMAND stress_test_ring_buffer)

add_executable(stress_test_pid tests/stress_test_pid.cpp)
target_link_libraries(stress_test_pid PRIVATE Threads::Threads)
target_include_directories(stress_test_pid PRIVATE src)
add_test(NAME stress_pid COMMAND stress_test_pid)

add_executable(test_watchdog tests/test_watchdog.cpp)
target_link_libraries(test_watchdog PRIVATE Threads::Threads)
target_include_directories(test_watchdog PRIVATE src)
add_test(NAME watchdog COMMAND test_watchdog)

add_executable(stress_test_watchdog tests/stress_test_watchdog.cpp)
target_link_libraries(stress_test_watchdog PRIVATE Threads::Threads)
target_include_directories(stress_test_watchdog PRIVATE src)
add_test(NAME stress_watchdog COMMAND stress_test_watchdog)

add_executable(test_hw_interfaces tests/test_hw_interfaces.cpp)
target_link_libraries(test_hw_interfaces PRIVATE Threads::Threads)
target_include_directories(test_hw_interfaces PRIVATE src)
add_test(NAME hw_interfaces COMMAND test_hw_interfaces)

add_executable(stress_test_hw_interfaces tests/stress_test_hw_interfaces.cpp)
target_link_libraries(stress_test_hw_interfaces PRIVATE Threads::Threads)
target_include_directories(stress_test_hw_interfaces PRIVATE src)
add_test(NAME stress_hw_interfaces COMMAND stress_test_hw_interfaces)

add_executable(test_noise_sim tests/test_noise_sim.cpp)
target_link_libraries(test_noise_sim PRIVATE Threads::Threads)
target_include_directories(test_noise_sim PRIVATE src)
add_test(NAME noise_sim COMMAND test_noise_sim)

add_executable(stress_test_noise_sim tests/stress_test_noise_sim.cpp)
target_link_libraries(stress_test_noise_sim PRIVATE Threads::Threads)
target_include_directories(stress_test_noise_sim PRIVATE src)
add_test(NAME stress_noise_sim COMMAND stress_test_noise_sim)

add_executable(test_hw_components tests/test_hw_components.cpp)
target_link_libraries(test_hw_components PRIVATE Threads::Threads)
target_include_directories(test_hw_components PRIVATE src)
add_test(NAME hw_components COMMAND test_hw_components)

add_executable(stress_test_hw_components tests/stress_test_hw_components.cpp)
target_link_libraries(stress_test_hw_components PRIVATE Threads::Threads)
target_include_directories(stress_test_hw_components PRIVATE src)
add_test(NAME stress_hw_components COMMAND stress_test_hw_components)

# Additional tests (will be uncommented as we implement components)
# add_executable(test_limits tests/test_limits.cpp)
# target_link_libraries(test_limits PRIVATE Threads::Threads)
# target_include_directories(test_limits PRIVATE src)
# add_test(NAME limits COMMAND test_limits)

# Tools (will be added later)
# add_executable(log_replay tools/log_replay.cpp)
