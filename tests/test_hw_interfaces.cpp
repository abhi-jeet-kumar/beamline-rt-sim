#include "../src/hw/isensor.hpp"
#include "../src/hw/iactuator.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <random>
#include <cmath>

/**
 * @brief Test implementations for abstract interfaces
 */

// Mock sensor for testing
class MockSensor : public ISensor {
private:
    double simulated_value_{0.0};
    bool simulate_error_{false};
    ErrorState error_to_simulate_{ErrorState::OK};
    std::mt19937 rng_{std::random_device{}()};
    std::normal_distribution<double> noise_{0.0, 0.01};
    
public:
    MockSensor(const std::string& id = "mock_sensor") {
        set_id(id);
    }
    
    double read() override {
        if (simulate_error_) {
            throw std::runtime_error("Simulated sensor error");
        }
        return simulated_value_ + noise_(rng_);
    }
    
    void set_simulated_value(double value) { simulated_value_ = value; }
    void set_simulate_error(bool error, ErrorState error_type = ErrorState::UNKNOWN_ERROR) {
        simulate_error_ = error;
        error_to_simulate_ = error_type;
    }
    
    std::string get_type_name() const override { return "MockSensor"; }
    std::string get_units() const override { return "units"; }
    std::pair<double, double> get_range() const override { return {-100.0, 100.0}; }
    double get_resolution() const override { return 0.001; }
    
    bool self_test() override {
        return !simulate_error_;
    }
};

// Mock actuator for testing
class MockActuator : public IActuator {
private:
    double current_value_{0.0};
    bool simulate_error_{false};
    ErrorState error_to_simulate_{ErrorState::OK};
    double response_time_us_{10.0}; // Simulated response time
    
public:
    MockActuator(const std::string& id = "mock_actuator") {
        set_id(id);
        set_limits(-10.0, 10.0);
        set_rate_limit(100.0); // 100 units/sec max rate
    }
    
    void set(double value) override {
        if (simulate_error_) {
            throw std::runtime_error("Simulated actuator error");
        }
        
        // Simulate response time
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(response_time_us_)));
        current_value_ = value;
    }
    
    double get() const override {
        return current_value_;
    }
    
    void set_simulated_response_time(double time_us) { response_time_us_ = time_us; }
    void set_simulate_error(bool error, ErrorState error_type = ErrorState::UNKNOWN_ERROR) {
        simulate_error_ = error;
        error_to_simulate_ = error_type;
    }
    
    std::string get_type_name() const override { return "MockActuator"; }
    std::string get_units() const override { return "units"; }
    double get_resolution() const override { return 0.001; }
    
    bool self_test() override {
        return !simulate_error_;
    }
};

/**
 * @brief Unit tests for hardware interfaces
 */
int main() {
    std::cout << "Testing Hardware Interfaces (ISensor & IActuator)..." << std::endl;
    
    // Test 1: ISensor basic functionality
    {
        std::cout << "Test 1: ISensor basic functionality" << std::endl;
        
        MockSensor sensor("test_sensor");
        sensor.set_simulated_value(5.0);
        
        // Test basic reading
        double value = sensor.read();
        assert(std::abs(value - 5.0) < 0.1); // Allow for noise
        
        // Test metadata reading
        auto reading = sensor.read_with_metadata();
        assert(reading.valid);
        assert(reading.error == ISensor::ErrorState::OK);
        assert(std::abs(reading.value - 5.0) < 0.1);
        assert(reading.quality == 1.0);
        assert(reading.is_fresh());
        
        // Test interface methods
        assert(sensor.get_id() == "test_sensor");
        assert(sensor.get_type_name() == "MockSensor");
        assert(sensor.get_units() == "units");
        assert(sensor.get_resolution() == 0.001);
        
        auto range = sensor.get_range();
        assert(range.first == -100.0);
        assert(range.second == 100.0);
        
        assert(sensor.is_healthy());
        assert(sensor.self_test());
        
        std::cout << "  ISensor basic functionality test passed" << std::endl;
    }
    
    // Test 2: ISensor error handling
    {
        std::cout << "Test 2: ISensor error handling" << std::endl;
        
        MockSensor sensor;
        sensor.set_simulate_error(true, ISensor::ErrorState::HARDWARE_FAULT);
        
        auto reading = sensor.read_with_metadata();
        assert(!reading.valid);
        assert(reading.error == ISensor::ErrorState::UNKNOWN_ERROR); // Caught by wrapper
        assert(reading.quality == 0.0);
        assert(!reading.is_fresh());
        
        assert(sensor.get_last_error() == ISensor::ErrorState::UNKNOWN_ERROR);
        assert(!sensor.is_healthy());
        assert(!sensor.self_test());
        
        // Test error string conversion
        std::string error_str = ISensor::error_to_string(ISensor::ErrorState::HARDWARE_FAULT);
        assert(error_str == "HARDWARE_FAULT");
        
        std::cout << "  ISensor error handling test passed" << std::endl;
    }
    
    // Test 3: ISensor statistics
    {
        std::cout << "Test 3: ISensor statistics" << std::endl;
        
        MockSensor sensor;
        sensor.set_simulated_value(1.0);
        
        // Perform multiple reads
        for (int i = 0; i < 100; i++) {
            sensor.read_with_metadata();
        }
        
        auto stats = sensor.get_statistics();
        assert(stats.total_reads == 100);
        assert(stats.successful_reads == 100);
        assert(stats.error_count == 0);
        assert(stats.success_rate == 100.0);
        assert(stats.mean_read_time_us > 0.0);
        
        // Add some errors
        sensor.set_simulate_error(true);
        for (int i = 0; i < 10; i++) {
            sensor.read_with_metadata();
        }
        
        stats = sensor.get_statistics();
        assert(stats.total_reads == 110);
        assert(stats.successful_reads == 100);
        assert(stats.error_count == 10);
        assert(std::abs(stats.success_rate - 90.91) < 0.1); // 100/110
        
        std::cout << "  Statistics: " << stats.total_reads << " reads, "
                  << stats.success_rate << "% success" << std::endl;
        
        std::cout << "  ISensor statistics test passed" << std::endl;
    }
    
    // Test 4: IActuator basic functionality
    {
        std::cout << "Test 4: IActuator basic functionality" << std::endl;
        
        MockActuator actuator("test_actuator");
        assert(actuator.initialize());
        
        // Test basic set/get
        actuator.set(3.5);
        assert(std::abs(actuator.get() - 3.5) < 1e-9);
        
        // Test with result reporting
        auto result = actuator.set_with_result(7.2);
        assert(result.success);
        assert(std::abs(result.actual_value - 7.2) < 1e-9);
        assert(std::abs(result.commanded_value - 7.2) < 1e-9);
        assert(result.error == IActuator::ErrorState::OK);
        assert(result.execution_time_us > 0.0);
        
        // Test interface methods
        assert(actuator.get_id() == "test_actuator");
        assert(actuator.get_type_name() == "MockActuator");
        assert(actuator.get_units() == "units");
        assert(actuator.get_resolution() == 0.001);
        
        auto limits = actuator.get_limits();
        assert(limits.first == -10.0);
        assert(limits.second == 10.0);
        assert(actuator.get_rate_limit() == 100.0);
        
        assert(actuator.is_healthy());
        assert(actuator.is_at_target(0.01));
        assert(std::abs(actuator.get_target() - 7.2) < 1e-9);
        assert(actuator.self_test());
        
        std::cout << "  IActuator basic functionality test passed" << std::endl;
    }
    
    // Test 5: IActuator safety limits
    {
        std::cout << "Test 5: IActuator safety limits" << std::endl;
        
        MockActuator actuator;
        actuator.initialize();
        actuator.set_limits(-5.0, 5.0);
        
        // Test out of range (positive)
        auto result = actuator.set_with_result(10.0);
        assert(!result.success);
        assert(result.error == IActuator::ErrorState::OUT_OF_RANGE);
        assert(actuator.get_last_error() == IActuator::ErrorState::OUT_OF_RANGE);
        
        // Test out of range (negative)
        result = actuator.set_with_result(-10.0);
        assert(!result.success);
        assert(result.error == IActuator::ErrorState::OUT_OF_RANGE);
        
        // Test valid range
        result = actuator.set_with_result(3.0);
        assert(result.success);
        assert(result.error == IActuator::ErrorState::OK);
        
        std::cout << "  IActuator safety limits test passed" << std::endl;
    }
    
    // Test 6: IActuator rate limiting
    {
        std::cout << "Test 6: IActuator rate limiting" << std::endl;
        
        MockActuator actuator;
        actuator.initialize();
        actuator.set_limits(-100.0, 100.0);
        actuator.set_rate_limit(10.0); // 10 units/sec max
        
        // Set initial value
        actuator.set_with_result(0.0);
        
        // Immediate large change should be rate limited
        auto result = actuator.set_with_result(50.0);
        assert(!result.success);
        assert(result.error == IActuator::ErrorState::RATE_LIMIT_EXCEEDED);
        
        // Wait and try smaller change
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        result = actuator.set_with_result(1.0); // Should be within rate limit
        assert(result.success);
        
        std::cout << "  IActuator rate limiting test passed" << std::endl;
    }
    
    // Test 7: IActuator error handling
    {
        std::cout << "Test 7: IActuator error handling" << std::endl;
        
        MockActuator actuator;
        actuator.set_simulate_error(true, IActuator::ErrorState::HARDWARE_FAULT);
        actuator.initialize();
        
        auto result = actuator.set_with_result(1.0);
        assert(!result.success);
        assert(result.error == IActuator::ErrorState::UNKNOWN_ERROR); // Caught by wrapper
        
        assert(actuator.get_last_error() == IActuator::ErrorState::UNKNOWN_ERROR);
        assert(!actuator.self_test());
        
        // Test error string conversion
        std::string error_str = IActuator::error_to_string(IActuator::ErrorState::RATE_LIMIT_EXCEEDED);
        assert(error_str == "RATE_LIMIT_EXCEEDED");
        
        std::cout << "  IActuator error handling test passed" << std::endl;
    }
    
    // Test 8: IActuator statistics
    {
        std::cout << "Test 8: IActuator statistics" << std::endl;
        
        MockActuator actuator;
        actuator.initialize();
        actuator.set_simulated_response_time(50.0); // 50μs response time
        
        // Perform multiple commands
        for (int i = 0; i < 50; i++) {
            actuator.set_with_result(static_cast<double>(i % 10));
        }
        
        auto stats = actuator.get_statistics();
        assert(stats.total_commands == 50);
        assert(stats.successful_commands == 50);
        assert(stats.error_count == 0);
        assert(stats.success_rate == 100.0);
        assert(stats.mean_command_time_us > 40.0); // Should be around 50μs
        assert(stats.min_commanded == 0.0);
        assert(stats.max_commanded == 9.0);
        
        // Add some range violations
        for (int i = 0; i < 5; i++) {
            actuator.set_with_result(100.0); // Out of range
        }
        
        stats = actuator.get_statistics();
        assert(stats.total_commands == 55);
        assert(stats.successful_commands == 50);
        assert(stats.error_count == 5);
        assert(stats.range_violations == 5);
        assert(std::abs(stats.success_rate - 90.91) < 0.1); // 50/55
        
        std::cout << "  Statistics: " << stats.total_commands << " commands, "
                  << stats.success_rate << "% success" << std::endl;
        
        std::cout << "  IActuator statistics test passed" << std::endl;
    }
    
    // Test 9: Initialization and shutdown
    {
        std::cout << "Test 9: Initialization and shutdown" << std::endl;
        
        MockSensor sensor;
        MockActuator actuator;
        
        // Initially not initialized
        assert(!sensor.is_initialized());
        assert(!actuator.is_initialized());
        
        // Initialize
        assert(sensor.initialize());
        assert(actuator.initialize());
        assert(sensor.is_initialized());
        assert(actuator.is_initialized());
        
        // Shutdown
        sensor.shutdown();
        actuator.shutdown();
        assert(!sensor.is_initialized());
        assert(!actuator.is_initialized());
        
        std::cout << "  Initialization and shutdown test passed" << std::endl;
    }
    
    // Test 10: Emergency stop and safety
    {
        std::cout << "Test 10: Emergency stop and safety" << std::endl;
        
        MockActuator actuator;
        actuator.initialize();
        
        // Set to some value
        actuator.set_with_result(5.0);
        assert(std::abs(actuator.get() - 5.0) < 1e-9);
        
        // Emergency stop should hold current position
        actuator.emergency_stop();
        double stopped_value = actuator.get();
        assert(std::abs(stopped_value - 5.0) < 1e-9);
        assert(actuator.is_at_target(0.01));
        
        std::cout << "  Emergency stop and safety test passed" << std::endl;
    }
    
    std::cout << "\n✅ All Hardware Interface tests passed!" << std::endl;
    return 0;
}
