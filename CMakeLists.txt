cmake_minimum_required(VERSION 3.20)
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

# Header-only libraries placeholder
# Will add nlohmann/json when implementing the RT loop

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

# Additional tests (will be uncommented as we implement components)
# add_executable(test_limits tests/test_limits.cpp)
# target_link_libraries(test_limits PRIVATE Threads::Threads)
# target_include_directories(test_limits PRIVATE src)
# add_test(NAME limits COMMAND test_limits)

# Tools (will be added later)
# add_executable(log_replay tools/log_replay.cpp)
