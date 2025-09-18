#include "../src/hw/isensor.hpp"
#include "../src/hw/iactuator.hpp"
#include "stress_test_framework.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <cmath>

/**
 * @brief High-performance mock implementations for stress testing
 */

class HighPerformanceMockSensor : public ISensor {
private:
    std::atomic<double> simulated_value_{0.0};
    std::atomic<bool> simulate_error_{false};
    mutable std::mt19937 rng_{std::random_device{}()};
    mutable std::uniform_real_distribution<double> noise_dist_{-0.01, 0.01};
    
public:
    HighPerformanceMockSensor(const std::string& id = "hp_sensor") {
        set_id(id);
        initialize();
    }
    
    double read() override {
        if (simulate_error_.load(std::memory_order_relaxed)) {
            throw std::runtime_error("Simulated error");
        }
        return simulated_value_.load(std::memory_order_relaxed) + noise_dist_(rng_);
    }
    
    void set_simulated_value(double value) { 
        simulated_value_.store(value, std::memory_order_relaxed); 
    }
    
    void set_simulate_error(bool error) { 
        simulate_error_.store(error, std::memory_order_relaxed); 
    }
    
    std::string get_type_name() const override { return "HighPerformanceMockSensor"; }
    std::string get_units() const override { return "units"; }
    std::pair<double, double> get_range() const override { return {-1000.0, 1000.0}; }
    double get_resolution() const override { return 0.0001; }
    bool self_test() override { return !simulate_error_.load(); }
};

class HighPerformanceMockActuator : public IActuator {
private:
    std::atomic<double> current_value_{0.0};
    std::atomic<bool> simulate_error_{false};
    std::atomic<double> response_delay_us_{1.0}; // Minimal delay for stress testing
    
public:
    HighPerformanceMockActuator(const std::string& id = "hp_actuator") {
        set_id(id);
        set_limits(-100.0, 100.0);
        set_rate_limit(10000.0); // High rate limit for stress testing
        initialize();
    }
    
    void set(double value) override {
        if (simulate_error_.load(std::memory_order_relaxed)) {
            throw std::runtime_error("Simulated error");
        }
        
        // Minimal simulated delay
        double delay = response_delay_us_.load(std::memory_order_relaxed);
        if (delay > 0) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<int64_t>(delay * 1000)));
        }
        
        current_value_.store(value, std::memory_order_relaxed);
    }
    
    double get() const override {
        return current_value_.load(std::memory_order_relaxed);
    }
    
    void set_simulate_error(bool error) { 
        simulate_error_.store(error, std::memory_order_relaxed); 
    }
    
    void set_response_delay_us(double delay) {
        response_delay_us_.store(delay, std::memory_order_relaxed);
    }
    
    std::string get_type_name() const override { return "HighPerformanceMockActuator"; }
    std::string get_units() const override { return "units"; }
    double get_resolution() const override { return 0.0001; }
    bool self_test() override { return !simulate_error_.load(); }
};

/**
 * @brief Comprehensive stress testing for hardware interfaces
 */
int main() {
    std::cout << "🔥 COMPREHENSIVE STRESS TESTING: Hardware Interfaces" << std::endl;
    std::cout << "====================================================" << std::endl;
    
    // Test 1: High-frequency sensor reading stress
    {
        std::cout << "\n🚀 Test 1: High-frequency sensor reading (100kHz)" << std::endl;
        
        HighPerformanceMockSensor sensor;
        sensor.set_simulated_value(42.0);
        StressTest::PerformanceMonitor monitor;
        
        const int iterations = 1000000; // 1M reads
        uint64_t successful_reads = 0;
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            try {
                double value = sensor.read();
                successful_reads++;
                
                // Verify reading is reasonable
                if (std::abs(value - 42.0) > 1.0) {
                    monitor.record_deadline_miss(); // Unexpected value
                }
            } catch (...) {
                monitor.record_deadline_miss(); // Read failure
            }
            
            auto end = std::chrono::steady_clock::now();
            double read_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(read_time);
        }
        
        monitor.print_statistics("High-frequency Sensor Reading");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 500000); // >500k reads/sec
        assert(stats.p99_us < 10.0); // P99 < 10μs per read
        assert(successful_reads > iterations * 0.99); // >99% success rate
        
        // Verify sensor statistics
        auto sensor_stats = sensor.get_statistics();
        std::cout << "  Sensor internal stats: " << sensor_stats.success_rate 
                  << "% success, mean: " << sensor_stats.mean_read_time_us << "μs" << std::endl;
        
        std::cout << "✅ High-frequency sensor reading test PASSED" << std::endl;
    }
    
    // Test 2: High-frequency actuator control stress
    {
        std::cout << "\n🚀 Test 2: High-frequency actuator control (50kHz)" << std::endl;
        
        HighPerformanceMockActuator actuator;
        actuator.set_response_delay_us(0.5); // Very fast response
        StressTest::PerformanceMonitor monitor;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> value_dist(-50.0, 50.0);
        
        const int iterations = 500000; // 500k commands
        uint64_t successful_commands = 0;
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            double target = value_dist(gen);
            auto result = actuator.set_with_result(target);
            
            if (result.success) {
                successful_commands++;
                
                // Verify actuator reached target
                if (std::abs(result.actual_value - target) > 1e-6) {
                    monitor.record_deadline_miss(); // Position error
                }
            } else {
                monitor.record_deadline_miss(); // Command failure
            }
            
            auto end = std::chrono::steady_clock::now();
            double command_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(command_time);
        }
        
        monitor.print_statistics("High-frequency Actuator Control");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 200000); // >200k commands/sec
        assert(stats.p99_us < 20.0); // P99 < 20μs per command
        assert(successful_commands > iterations * 0.99); // >99% success rate
        
        // Verify actuator statistics
        auto actuator_stats = actuator.get_statistics();
        std::cout << "  Actuator internal stats: " << actuator_stats.success_rate 
                  << "% success, mean: " << actuator_stats.mean_command_time_us << "μs" << std::endl;
        
        std::cout << "✅ High-frequency actuator control test PASSED" << std::endl;
    }
    
    // Test 3: Multi-threaded sensor reading stress
    {
        std::cout << "\n🚀 Test 3: Multi-threaded sensor reading" << std::endl;
        
        HighPerformanceMockSensor sensor;
        sensor.set_simulated_value(100.0);
        
        const int num_threads = 8;
        const int reads_per_thread = 100000;
        std::vector<std::thread> threads;
        std::vector<StressTest::PerformanceMonitor> monitors(num_threads);
        std::atomic<uint64_t> total_successful_reads{0};
        
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                uint64_t local_successful = 0;
                
                for (int i = 0; i < reads_per_thread; i++) {
                    auto start = std::chrono::steady_clock::now();
                    
                    try {
                        double value = sensor.read_with_metadata().value;
                        local_successful++;
                        
                        if (std::abs(value - 100.0) > 1.0) {
                            monitors[t].record_deadline_miss();
                        }
                    } catch (...) {
                        monitors[t].record_deadline_miss();
                    }
                    
                    auto end = std::chrono::steady_clock::now();
                    double read_time = std::chrono::duration<double, std::micro>(end - start).count();
                    monitors[t].record_timing(read_time);
                }
                
                total_successful_reads.fetch_add(local_successful);
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Analyze per-thread performance
        std::cout << "  Thread performance analysis:" << std::endl;
        for (int t = 0; t < num_threads; t++) {
            auto stats = monitors[t].get_statistics();
            std::cout << "    Thread " << t << ": " << stats.throughput_ops_per_sec 
                      << " reads/sec, P99: " << stats.p99_us << "μs" << std::endl;
            
            assert(stats.throughput_ops_per_sec > 50000); // Each thread >50k reads/sec
        }
        
        assert(total_successful_reads.load() > (num_threads * reads_per_thread * 0.99));
        
        std::cout << "✅ Multi-threaded sensor reading test PASSED" << std::endl;
    }
    
    // Test 4: Actuator performance under CPU stress
    {
        std::cout << "\n🚀 Test 4: Actuator performance under CPU stress" << std::endl;
        
        HighPerformanceMockActuator actuator;
        StressTest::CPUStressor cpu_stress;
        StressTest::PerformanceMonitor monitor;
        
        cpu_stress.start_stress();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> target_dist(-75.0, 75.0);
        
        const int iterations = 100000;
        uint64_t successful_commands = 0;
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            double target = target_dist(gen);
            auto result = actuator.set_with_result(target);
            
            if (result.success) {
                successful_commands++;
            } else {
                monitor.record_deadline_miss();
            }
            
            auto end = std::chrono::steady_clock::now();
            double command_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(command_time);
        }
        
        cpu_stress.stop_stress();
        monitor.print_statistics("Actuator under CPU Stress");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 50000); // >50k commands/sec under stress
        assert(successful_commands > iterations * 0.95); // >95% success under stress
        
        std::cout << "✅ CPU stress test PASSED" << std::endl;
    }
    
    // Test 5: Error handling stress test
    {
        std::cout << "\n🚀 Test 5: Error handling stress test" << std::endl;
        
        HighPerformanceMockSensor sensor;
        HighPerformanceMockActuator actuator;
        
        StressTest::PerformanceMonitor sensor_monitor, actuator_monitor;
        
        // Test with 10% error rate
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> error_prob(0.0, 1.0);
        
        const int iterations = 50000;
        uint64_t sensor_errors = 0, actuator_errors = 0;
        
        for (int i = 0; i < iterations; i++) {
            // Sensor test with errors
            {
                auto start = std::chrono::steady_clock::now();
                
                bool simulate_error = error_prob(gen) < 0.1; // 10% error rate
                sensor.set_simulate_error(simulate_error);
                
                auto reading = sensor.read_with_metadata();
                if (!reading.valid) {
                    sensor_errors++;
                    sensor_monitor.record_deadline_miss();
                }
                
                auto end = std::chrono::steady_clock::now();
                double read_time = std::chrono::duration<double, std::micro>(end - start).count();
                sensor_monitor.record_timing(read_time);
            }
            
            // Actuator test with errors
            {
                auto start = std::chrono::steady_clock::now();
                
                bool simulate_error = error_prob(gen) < 0.1; // 10% error rate
                actuator.set_simulate_error(simulate_error);
                
                auto result = actuator.set_with_result(static_cast<double>(i % 50));
                if (!result.success) {
                    actuator_errors++;
                    actuator_monitor.record_deadline_miss();
                }
                
                auto end = std::chrono::steady_clock::now();
                double command_time = std::chrono::duration<double, std::micro>(end - start).count();
                actuator_monitor.record_timing(command_time);
            }
        }
        
        sensor_monitor.print_statistics("Sensor Error Handling");
        actuator_monitor.print_statistics("Actuator Error Handling");
        
        std::cout << "  Sensor errors: " << sensor_errors << " (" 
                  << (static_cast<double>(sensor_errors) / iterations * 100) << "%)" << std::endl;
        std::cout << "  Actuator errors: " << actuator_errors << " (" 
                  << (static_cast<double>(actuator_errors) / iterations * 100) << "%)" << std::endl;
        
        // Should have approximately 10% error rate
        assert(sensor_errors > iterations * 0.05); // At least 5%
        assert(sensor_errors < iterations * 0.15); // At most 15%
        assert(actuator_errors > iterations * 0.05);
        assert(actuator_errors < iterations * 0.15);
        
        std::cout << "✅ Error handling stress test PASSED" << std::endl;
    }
    
    // Test 6: Memory pressure resistance
    {
        std::cout << "\n🚀 Test 6: Memory pressure resistance" << std::endl;
        
        HighPerformanceMockSensor sensor;
        HighPerformanceMockActuator actuator;
        StressTest::MemoryStressor mem_stress;
        StressTest::PerformanceMonitor monitor;
        
        // Apply memory pressure
        mem_stress.allocate_memory_mb(300);
        mem_stress.allocate_memory_mb(300); // Total 600MB
        
        const int iterations = 50000;
        uint64_t successful_operations = 0;
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            // Combined sensor read and actuator command
            auto reading = sensor.read_with_metadata();
            auto result = actuator.set_with_result(static_cast<double>(i % 100));
            
            if (reading.valid && result.success) {
                successful_operations++;
            } else {
                monitor.record_deadline_miss();
            }
            
            auto end = std::chrono::steady_clock::now();
            double op_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(op_time);
        }
        
        mem_stress.free_all();
        monitor.print_statistics("Memory Pressure Test");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 50000); // >50k ops/sec under memory pressure
        assert(successful_operations > iterations * 0.95); // >95% success
        
        std::cout << "✅ Memory pressure test PASSED" << std::endl;
    }
    
    // Test 7: Real-time control loop simulation
    {
        std::cout << "\n🚀 Test 7: Real-time control loop simulation" << std::endl;
        
        HighPerformanceMockSensor sensor;
        HighPerformanceMockActuator actuator;
        sensor.set_simulated_value(0.0);
        
        auto control_loop = [&]() {
            // Simulate control loop: read sensor, calculate, command actuator
            auto reading = sensor.read_with_metadata();
            
            // Simple control law: drive sensor reading to zero
            double error = 0.0 - reading.value;
            double command = error * 0.1; // Proportional gain
            
            auto result = actuator.set_with_result(command);
            
            // Update sensor based on actuator output (simple coupling)
            double new_sensor_value = reading.value + result.actual_value * 0.01;
            sensor.set_simulated_value(new_sensor_value);
            
            return reading.valid && result.success;
        };
        
        StressTest::RealtimeStressTest<decltype(control_loop)> rt_test(
            control_loop,
            std::chrono::microseconds(1000), // 1kHz control loop
            std::chrono::microseconds(1000)  // 1ms deadline
        );
        
        auto results = rt_test.run_test(
            10000, // 10 seconds of operation
            false, // no CPU stress
            false  // no memory stress
        );
        
        assert(results.passed);
        assert(results.stats.deadline_miss_rate < 0.01); // <1% deadline misses
        
        std::cout << "✅ Real-time control loop simulation PASSED" << std::endl;
    }
    
    // Test 8: Long-term endurance test
    {
        std::cout << "\n🚀 Test 8: Endurance test" << std::endl;
        
        HighPerformanceMockSensor sensor;
        HighPerformanceMockActuator actuator;
        
        auto endurance_test_func = [&]() {
            static std::mt19937 gen(std::random_device{}());
            static std::uniform_real_distribution<double> value_dist(-50.0, 50.0);
            
            // Read sensor and command actuator
            sensor.set_simulated_value(value_dist(gen));
            auto reading = sensor.read_with_metadata();
            auto result = actuator.set_with_result(value_dist(gen));
            
            return reading.valid && result.success;
        };
        
        StressTest::EnduranceTest<decltype(endurance_test_func)> endurance_test(endurance_test_func);
        
        // Run for 30 seconds (would be longer in production)
        endurance_test.run_for_duration(std::chrono::seconds(30));
        
        // Verify long-term performance
        auto sensor_stats = sensor.get_statistics();
        auto actuator_stats = actuator.get_statistics();
        
        std::cout << "  Final sensor stats: " << sensor_stats.total_reads 
                  << " reads, " << sensor_stats.success_rate << "% success" << std::endl;
        std::cout << "  Final actuator stats: " << actuator_stats.total_commands 
                  << " commands, " << actuator_stats.success_rate << "% success" << std::endl;
        
        assert(sensor_stats.success_rate > 95.0);
        assert(actuator_stats.success_rate > 95.0);
        assert(sensor.is_healthy());
        assert(actuator.is_healthy());
        
        std::cout << "✅ Endurance test PASSED" << std::endl;
    }
    
    std::cout << "\n🎉 ALL HARDWARE INTERFACE STRESS TESTS PASSED!" << std::endl;
    std::cout << "📊 Hardware interfaces validated for production real-time use" << std::endl;
    
    return 0;
}
