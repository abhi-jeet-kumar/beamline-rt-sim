#include "../src/core/telemetry.hpp"
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>

/**
 * @brief Test telemetry data structures and utility functions
 * 
 * This test suite verifies:
 * 1. TelemetrySample construction and default values
 * 2. Utility functions for health checking and formatting
 * 3. Timestamp generation from steady clock
 * 4. Extended telemetry structure inheritance
 * 5. Statistics structure functionality
 * 6. String formatting and data integrity
 */

int main() {
    std::cout << "Testing telemetry data structures..." << std::endl;
    
    // Test 1: Basic TelemetrySample construction and defaults
    {
        std::cout << "Test 1: TelemetrySample construction" << std::endl;
        
        TelemetrySample sample;
        
        // Check default values
        assert(sample.t_sec == 0.0);
        assert(sample.cycle == 0);
        assert(sample.pos == 0.0);
        assert(sample.intensity == 0.0);
        assert(sample.magnet_current == 0.0);
        assert(sample.setpoint == 0.0);
        assert(sample.error == 0.0);
        assert(sample.pid_p == 0.0);
        assert(sample.pid_i == 0.0);
        assert(sample.pid_d == 0.0);
        assert(sample.control_output == 0.0);
        assert(sample.deadline_miss == false);
        assert(sample.magnet_saturated == false);
        assert(sample.integrator_saturated == false);
        assert(sample.loop_time_us == 0);
        assert(sample.cpu_usage == 0.0);
        
        std::cout << "  Default construction test passed" << std::endl;
    }
    
    // Test 2: Utility functions
    {
        std::cout << "Test 2: Utility functions" << std::endl;
        
        TelemetrySample sample;
        
        // Test is_healthy() with default (healthy) state
        assert(sample.is_healthy() == true);
        
        // Test health with various error conditions
        sample.deadline_miss = true;
        assert(sample.is_healthy() == false);
        
        sample.deadline_miss = false;
        sample.magnet_saturated = true;
        assert(sample.is_healthy() == false);
        
        sample.magnet_saturated = false;
        sample.integrator_saturated = true;
        assert(sample.is_healthy() == false);
        
        // Reset to healthy
        sample.integrator_saturated = false;
        assert(sample.is_healthy() == true);
        
        std::cout << "  Health checking test passed" << std::endl;
    }
    
    // Test 3: PID total calculation
    {
        std::cout << "Test 3: PID total calculation" << std::endl;
        
        TelemetrySample sample;
        sample.pid_p = 1.5;
        sample.pid_i = -0.3;
        sample.pid_d = 0.2;
        
        double expected_total = 1.5 - 0.3 + 0.2;
        assert(std::abs(sample.get_pid_total() - expected_total) < 1e-9);
        
        std::cout << "  PID total calculation test passed" << std::endl;
    }
    
    // Test 4: Position tolerance checking
    {
        std::cout << "Test 4: Position tolerance checking" << std::endl;
        
        TelemetrySample sample;
        
        // Test exact match
        sample.error = 0.0;
        assert(sample.position_in_tolerance(0.1) == true);
        
        // Test within tolerance
        sample.error = 0.05;
        assert(sample.position_in_tolerance(0.1) == true);
        
        sample.error = -0.08;
        assert(sample.position_in_tolerance(0.1) == true);
        
        // Test outside tolerance
        sample.error = 0.15;
        assert(sample.position_in_tolerance(0.1) == false);
        
        sample.error = -0.12;
        assert(sample.position_in_tolerance(0.1) == false);
        
        std::cout << "  Position tolerance test passed" << std::endl;
    }
    
    // Test 5: Timestamp generation
    {
        std::cout << "Test 5: Timestamp generation" << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        // Wait a small amount
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        double timestamp = TelemetrySample::timestamp_from_steady_clock(start_time);
        
        // Should be approximately 10ms = 0.01s
        assert(timestamp > 0.005);  // At least 5ms
        assert(timestamp < 0.050);  // Less than 50ms (generous for slow systems)
        
        std::cout << "  Timestamp: " << timestamp << " seconds" << std::endl;
        std::cout << "  Timestamp generation test passed" << std::endl;
    }
    
    // Test 6: String formatting
    {
        std::cout << "Test 6: String formatting" << std::endl;
        
        TelemetrySample sample;
        sample.t_sec = 1.234;
        sample.cycle = 1234;
        sample.pos = 0.567;
        sample.intensity = 98765.4;
        sample.magnet_current = 2.345;
        sample.setpoint = 0.5;
        sample.error = 0.067;
        sample.pid_p = 0.1;
        sample.pid_i = 0.02;
        sample.pid_d = 0.003;
        sample.control_output = 0.123;
        sample.deadline_miss = false;
        sample.magnet_saturated = true;
        sample.integrator_saturated = false;
        sample.loop_time_us = 250;
        sample.cpu_usage = 0.456;
        
        std::string str = sample.to_string();
        
        // Check that key values appear in the string
        assert(str.find("1.234") != std::string::npos);  // timestamp
        assert(str.find("1234") != std::string::npos);   // cycle
        assert(str.find("0.567") != std::string::npos);  // position
        assert(str.find("OK") != std::string::npos);     // deadline OK
        assert(str.find("SAT") != std::string::npos);    // magnet saturated
        assert(str.find("250") != std::string::npos);    // loop time
        
        std::cout << "  Sample string: " << str.substr(0, 100) << "..." << std::endl;
        std::cout << "  String formatting test passed" << std::endl;
    }
    
    // Test 7: ExtendedTelemetrySample inheritance
    {
        std::cout << "Test 7: ExtendedTelemetrySample" << std::endl;
        
        ExtendedTelemetrySample ext_sample;
        
        // Check base class functionality works
        assert(ext_sample.is_healthy() == true);
        ext_sample.pid_p = 1.0;
        ext_sample.pid_i = 2.0;
        ext_sample.pid_d = 3.0;
        assert(std::abs(ext_sample.get_pid_total() - 6.0) < 1e-9);
        
        // Check extended fields have sensible defaults
        assert(ext_sample.bpm_noise_level == 0.0);
        assert(ext_sample.magnet_temperature == 25.0);  // Room temperature
        assert(ext_sample.power_supply_voltage == 0.0);
        assert(ext_sample.loop_jitter_us == 0.0);
        assert(ext_sample.missed_deadlines == 0);
        assert(ext_sample.integrator_value == 0.0);
        assert(ext_sample.telemetry_drops == 0);
        assert(ext_sample.command_latency_us == 0);
        
        // Test that it can be assigned to base class
        TelemetrySample* base_ptr = &ext_sample;
        base_ptr->pos = 1.23;
        assert(ext_sample.pos == 1.23);
        
        std::cout << "  ExtendedTelemetrySample test passed" << std::endl;
    }
    
    // Test 8: TelemetryStats functionality
    {
        std::cout << "Test 8: TelemetryStats" << std::endl;
        
        TelemetryStats stats;
        
        // Check default values
        assert(stats.sample_count == 0);
        assert(stats.pos_mean == 0.0);
        assert(stats.error_rms == 0.0);
        assert(stats.deadline_miss_count == 0);
        assert(stats.deadline_miss_rate == 0.0);
        
        // Test health checking with good stats
        assert(stats.is_healthy() == true);
        
        // Test with problematic stats
        stats.deadline_miss_rate = 0.02;  // 2% > 1% threshold
        assert(stats.is_healthy() == false);
        
        stats.deadline_miss_rate = 0.005;  // Back to good
        stats.max_loop_time_us = 600.0;    // > 500μs threshold
        assert(stats.is_healthy() == false);
        
        stats.max_loop_time_us = 300.0;    // Back to good
        stats.max_cpu_usage = 0.85;        // > 80% threshold
        assert(stats.is_healthy() == false);
        
        // Reset and verify
        stats.reset();
        assert(stats.sample_count == 0);
        assert(stats.max_cpu_usage == 0.0);
        assert(stats.is_healthy() == true);
        
        std::cout << "  TelemetryStats test passed" << std::endl;
    }
    
    // Test 9: Data structure sizes (for performance)
    {
        std::cout << "Test 9: Data structure sizes" << std::endl;
        
        std::cout << "  TelemetrySample size: " << sizeof(TelemetrySample) << " bytes" << std::endl;
        std::cout << "  ExtendedTelemetrySample size: " << sizeof(ExtendedTelemetrySample) << " bytes" << std::endl;
        std::cout << "  TelemetryStats size: " << sizeof(TelemetryStats) << " bytes" << std::endl;
        
        // Basic size sanity checks (should be reasonable for high-frequency use)
        assert(sizeof(TelemetrySample) < 200);  // Should be compact
        assert(sizeof(ExtendedTelemetrySample) < 300);
        assert(sizeof(TelemetryStats) < 200);
        
        std::cout << "  Data structure size test passed" << std::endl;
    }
    
    // Test 10: Real-world scenario simulation
    {
        std::cout << "Test 10: Real-world scenario" << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        TelemetrySample sample;
        
        // Simulate a realistic control loop sample
        sample.t_sec = TelemetrySample::timestamp_from_steady_clock(start_time);
        sample.cycle = 12345;
        sample.pos = 0.025;      // 25 μm beam position
        sample.intensity = 5432.1;
        sample.magnet_current = 1.234;
        sample.setpoint = 0.0;   // Centered beam
        sample.error = sample.setpoint - sample.pos;  // -25 μm error
        sample.pid_p = -0.025;   // Proportional response
        sample.pid_i = -0.001;   // Small integral
        sample.pid_d = 0.002;    // Derivative term
        sample.control_output = sample.get_pid_total();
        sample.loop_time_us = 150;  // 150 μs loop time
        sample.cpu_usage = 0.25;    // 25% CPU
        
        // Verify calculations
        assert(std::abs(sample.error - (-0.025)) < 1e-9);
        assert(std::abs(sample.get_pid_total() - (-0.024)) < 1e-9);
        assert(sample.position_in_tolerance(0.1) == true);  // Within 100 μm
        assert(sample.position_in_tolerance(0.01) == false); // Outside 10 μm
        assert(sample.is_healthy() == true);
        
        std::cout << "  Realistic sample: " << sample.to_string() << std::endl;
        std::cout << "  Real-world scenario test passed" << std::endl;
    }
    
    std::cout << "\n✅ All telemetry structure tests passed!" << std::endl;
    return 0;
}
