#include "../src/hw/bpm.hpp"
#include "../src/hw/bic.hpp"
#include "../src/hw/magnet.hpp"
#include "stress_test_framework.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
#include <thread>
#include <atomic>

/**
 * @brief Comprehensive stress testing for hardware components
 * 
 * Tests include:
 * 1. High-frequency component operation (kHz rates)
 * 2. Multi-threaded component access
 * 3. Long-term stability and drift
 * 4. Performance under system stress
 * 5. Real-time control loop simulation
 * 6. Memory and CPU pressure resistance
 * 7. Statistical accuracy validation
 * 8. Endurance testing
 */

int main() {
    std::cout << "🔥 COMPREHENSIVE STRESS TESTING: Hardware Components" << std::endl;
    std::cout << "====================================================" << std::endl;
    
    // Test 1: High-frequency BPM readout stress
    {
        std::cout << "\n🚀 Test 1: High-frequency BPM readout (10kHz)" << std::endl;
        
        BPM bpm("STRESS_BPM", 12345);
        bpm.initialize();
        bpm.set_beam_position(1.0, 0.5);
        bpm.set_readout_axis("X");
        bpm.enable_noise(true);
        
        StressTest::PerformanceMonitor monitor;
        const int iterations = 100000; // 100k reads at 10kHz
        uint64_t successful_reads = 0;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> pos_dist(-5.0, 5.0);
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            // Vary beam position dynamically
            if (i % 100 == 0) {
                double new_x = pos_dist(gen);
                double new_y = pos_dist(gen);
                bpm.set_beam_position(new_x, new_y);
            }
            
            try {
                double reading = bpm.read();
                successful_reads++;
                
                // Verify reading is reasonable
                if (std::abs(reading) > 20.0) { // Outside reasonable range
                    monitor.record_deadline_miss();
                }
            } catch (...) {
                monitor.record_deadline_miss();
            }
            
            auto end = std::chrono::steady_clock::now();
            double read_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(read_time);
            
            // Real-time requirement: <100μs per read
            if (read_time > 100.0) {
                monitor.record_deadline_miss();
            }
        }
        
        monitor.print_statistics("High-frequency BPM Readout");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 50000); // >50k reads/sec
        assert(stats.p99_us < 50.0); // P99 < 50μs per read
        assert(successful_reads > iterations * 0.99); // >99% success
        
        std::cout << "  BPM reads: " << bpm.get_read_count() << std::endl;
        std::cout << "✅ High-frequency BPM test PASSED" << std::endl;
    }
    
    // Test 2: High-frequency BIC measurement stress
    {
        std::cout << "\n🚀 Test 2: High-frequency BIC measurement (5kHz)" << std::endl;
        
        BIC bic("STRESS_BIC", 23456);
        bic.initialize();
        bic.enable_noise(true);
        
        StressTest::PerformanceMonitor monitor;
        const int iterations = 50000; // 50k measurements
        uint64_t successful_reads = 0;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> intensity_dist(100.0, 10000.0);
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            // Vary beam intensity
            if (i % 50 == 0) {
                double new_intensity = intensity_dist(gen);
                bic.set_beam_intensity(new_intensity);
            }
            
            try {
                double reading = bic.read();
                successful_reads++;
                
                // Verify reading is reasonable
                if (reading < 0.0 || reading > 50000.0) {
                    monitor.record_deadline_miss();
                }
            } catch (...) {
                monitor.record_deadline_miss();
            }
            
            auto end = std::chrono::steady_clock::now();
            double read_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(read_time);
        }
        
        monitor.print_statistics("High-frequency BIC Measurement");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 25000); // >25k measurements/sec
        assert(stats.p99_us < 100.0); // P99 < 100μs per measurement
        assert(successful_reads > iterations * 0.99); // >99% success
        
        std::cout << "✅ High-frequency BIC test PASSED" << std::endl;
    }
    
    // Test 3: High-frequency Magnet control stress
    {
        std::cout << "\n🚀 Test 3: High-frequency Magnet control (1kHz)" << std::endl;
        
        Magnet magnet("STRESS_MAG", 34567);
        magnet.initialize();
        magnet.enable_noise(true);
        magnet.set_slew_rate_limit(100.0); // High slew rate for stress test
        
        StressTest::PerformanceMonitor monitor;
        const int iterations = 10000; // 10k commands at 1kHz
        uint64_t successful_commands = 0;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> current_dist(-20.0, 20.0);
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            double target_current = current_dist(gen);
            
            try {
                auto result = magnet.set_with_result(target_current);
                if (result.success) {
                    successful_commands++;
                } else {
                    monitor.record_deadline_miss();
                }
                
                // Verify current is reasonable
                double actual = magnet.get();
                if (std::abs(actual) > 100.0) { // Unreasonable current
                    monitor.record_deadline_miss();
                }
            } catch (...) {
                monitor.record_deadline_miss();
            }
            
            auto end = std::chrono::steady_clock::now();
            double command_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(command_time);
        }
        
        monitor.print_statistics("High-frequency Magnet Control");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 5000); // >5k commands/sec
        assert(stats.p99_us < 200.0); // P99 < 200μs per command
        assert(successful_commands > iterations * 0.95); // >95% success (some rate limiting expected)
        
        std::cout << "  Magnet power: " << magnet.get_power() << "W" << std::endl;
        std::cout << "✅ High-frequency Magnet test PASSED" << std::endl;
    }
    
    // Test 4: Multi-threaded component access
    {
        std::cout << "\n🚀 Test 4: Multi-threaded component access" << std::endl;
        
        BPM bpm("MT_BPM", 45678);
        BIC bic("MT_BIC", 56789);
        Magnet magnet("MT_MAG", 67890);
        
        bpm.initialize();
        bic.initialize();
        magnet.initialize();
        
        const int num_threads = 4;
        const int ops_per_thread = 10000;
        std::vector<std::thread> threads;
        std::vector<StressTest::PerformanceMonitor> monitors(num_threads);
        std::atomic<uint64_t> total_operations{0};
        
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                std::mt19937 gen(1000 + t);
                std::uniform_real_distribution<double> dist(-10.0, 10.0);
                uint64_t local_ops = 0;
                
                for (int i = 0; i < ops_per_thread; i++) {
                    auto start = std::chrono::steady_clock::now();
                    
                    // Cycle through different component operations
                    switch (i % 3) {
                        case 0: {
                            // BPM operation
                            bpm.set_beam_position(dist(gen), dist(gen));
                            volatile double reading = bpm.read();
                            (void)reading;
                            break;
                        }
                        case 1: {
                            // BIC operation
                            bic.set_beam_intensity(std::abs(dist(gen)) * 100 + 100);
                            volatile double reading = bic.read();
                            (void)reading;
                            break;
                        }
                        case 2: {
                            // Magnet operation
                            magnet.set(dist(gen));
                            volatile double current = magnet.get();
                            (void)current;
                            break;
                        }
                    }
                    
                    local_ops++;
                    
                    auto end = std::chrono::steady_clock::now();
                    double op_time = std::chrono::duration<double, std::micro>(end - start).count();
                    monitors[t].record_timing(op_time);
                }
                
                total_operations.fetch_add(local_ops);
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
                      << " ops/sec, P99: " << stats.p99_us << "μs" << std::endl;
            
            assert(stats.throughput_ops_per_sec > 5000); // Each thread >5k ops/sec
        }
        
        assert(total_operations.load() == num_threads * ops_per_thread);
        
        std::cout << "✅ Multi-threaded access test PASSED" << std::endl;
    }
    
    // Test 5: System stress with CPU and memory pressure
    {
        std::cout << "\n🚀 Test 5: Components under system stress" << std::endl;
        
        BPM bpm("SYS_BPM", 78901);
        BIC bic("SYS_BIC", 89012);
        Magnet magnet("SYS_MAG", 90123);
        
        bpm.initialize();
        bic.initialize();
        magnet.initialize();
        
        StressTest::CPUStressor cpu_stress;
        StressTest::MemoryStressor mem_stress;
        StressTest::PerformanceMonitor monitor;
        
        // Apply stress
        cpu_stress.start_stress();
        mem_stress.allocate_memory_mb(200);
        mem_stress.allocate_memory_mb(200); // Total 400MB
        
        const int iterations = 20000;
        uint64_t successful_operations = 0;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(-5.0, 5.0);
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            try {
                // Combined operation: read sensors, control magnet
                bpm.set_beam_position(dist(gen), dist(gen));
                double bpm_reading = bpm.read();
                
                bic.set_beam_intensity(std::abs(dist(gen)) * 1000 + 500);
                double bic_reading = bic.read();
                
                // Simple control based on BPM reading
                double correction = -bpm_reading * 0.1;
                magnet.set(correction);
                double magnet_current = magnet.get();
                
                // Verify readings are reasonable
                if (std::abs(bpm_reading) < 50.0 && bic_reading > 0.0 && std::abs(magnet_current) < 100.0) {
                    successful_operations++;
                }
            } catch (...) {
                monitor.record_deadline_miss();
            }
            
            auto end = std::chrono::steady_clock::now();
            double op_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(op_time);
        }
        
        // Clean up stress
        cpu_stress.stop_stress();
        mem_stress.free_all();
        
        monitor.print_statistics("System Stress Test");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 2000); // >2k complete cycles/sec under stress
        assert(successful_operations > iterations * 0.95); // >95% success under stress
        
        std::cout << "✅ System stress test PASSED" << std::endl;
    }
    
    // Test 6: Real-time control loop simulation
    {
        std::cout << "\n🚀 Test 6: Real-time control loop simulation" << std::endl;
        
        BPM bpm("RT_BPM", 11111);
        Magnet magnet("RT_MAG", 22222);
        
        bpm.initialize();
        magnet.initialize();
        
        bpm.enable_noise(true);
        magnet.enable_noise(true);
        
        auto control_loop = [&]() {
            // Simulate real-time control: read BPM, calculate correction, set magnet
            
            // Read current beam position
            double position = bpm.read();
            
            // Simple proportional control (target position = 0)
            double error = 0.0 - position;
            double correction = error * 0.5; // Proportional gain
            
            // Apply correction to magnet
            magnet.set(correction);
            
            // Simulate beam response to magnet (simplified)
            double magnet_field = magnet.get_magnetic_field();
            double beam_deflection = magnet_field * 10.0; // Simplified beam optics
            auto [current_x, current_y] = bpm.get_beam_position();
            bpm.set_beam_position(current_x + beam_deflection * 0.01, current_y);
            
            return std::abs(position) < 10.0 && std::abs(correction) < 50.0;
        };
        
        StressTest::RealtimeStressTest<decltype(control_loop)> rt_test(
            control_loop,
            std::chrono::microseconds(1000), // 1kHz control loop
            std::chrono::microseconds(1000)  // 1ms deadline
        );
        
        auto results = rt_test.run_test(
            5000,  // 5 seconds of control
            false, // no additional CPU stress
            false  // no memory stress
        );
        
        assert(results.passed);
        assert(results.stats.deadline_miss_rate < 0.02); // <2% deadline misses
        
        std::cout << "✅ Real-time control loop simulation PASSED" << std::endl;
    }
    
    // Test 7: Long-term stability test
    {
        std::cout << "\n🚀 Test 7: Long-term stability test" << std::endl;
        
        BPM bpm("STABLE_BPM", 33333);
        BIC bic("STABLE_BIC", 44444);
        Magnet magnet("STABLE_MAG", 55555);
        
        bpm.initialize();
        bic.initialize();
        magnet.initialize();
        
        // Set stable conditions
        bpm.set_beam_position(1.0, 0.5);
        bic.set_beam_intensity(1000.0);
        magnet.set(2.0);
        
        // Record initial readings
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        double initial_bpm = bpm.read();
        double initial_bic = bic.read();
        double initial_magnet = magnet.get();
        
        // Run for extended period
        const int test_duration_ms = 30000; // 30 seconds
        const int sample_interval_ms = 100;  // Sample every 100ms
        
        std::vector<double> bpm_readings, bic_readings, magnet_readings;
        
        for (int t = 0; t < test_duration_ms; t += sample_interval_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms));
            
            bpm_readings.push_back(bpm.read());
            bic_readings.push_back(bic.read());
            magnet_readings.push_back(magnet.get());
        }
        
        // Analyze stability
        auto calc_stability = [](const std::vector<double>& data, double initial) {
            double max_dev = 0.0;
            for (double val : data) {
                max_dev = std::max(max_dev, std::abs(val - initial));
            }
            return max_dev / std::abs(initial) * 100.0; // Percentage deviation
        };
        
        double bpm_stability = calc_stability(bpm_readings, initial_bpm);
        double bic_stability = calc_stability(bic_readings, initial_bic);
        double magnet_stability = calc_stability(magnet_readings, initial_magnet);
        
        std::cout << "  BPM stability: " << bpm_stability << "%" << std::endl;
        std::cout << "  BIC stability: " << bic_stability << "%" << std::endl;
        std::cout << "  Magnet stability: " << magnet_stability << "%" << std::endl;
        
        // Stability requirements (allowing for noise)
        assert(bpm_stability < 50.0);    // <50% deviation for BPM (has noise)
        assert(bic_stability < 30.0);    // <30% deviation for BIC
        assert(magnet_stability < 10.0); // <10% deviation for Magnet
        
        std::cout << "✅ Long-term stability test PASSED" << std::endl;
    }
    
    // Test 8: Endurance test
    {
        std::cout << "\n🚀 Test 8: Endurance test" << std::endl;
        
        BPM bpm("END_BPM", 66666);
        BIC bic("END_BIC", 77777);
        Magnet magnet("END_MAG", 88888);
        
        bpm.initialize();
        bic.initialize();
        magnet.initialize();
        
        auto endurance_test_func = [&]() {
            static std::mt19937 gen(std::random_device{}());
            static std::uniform_real_distribution<double> dist(-2.0, 2.0);
            static int counter = 0;
            counter++;
            
            // Cycle through different operations
            switch (counter % 3) {
                case 0:
                    bpm.set_beam_position(dist(gen), dist(gen));
                    return bpm.read() != 0.0; // Non-zero reading expected
                case 1:
                    bic.set_beam_intensity(std::abs(dist(gen)) * 500 + 100);
                    return bic.read() > 0.0; // Positive intensity expected
                case 2:
                    magnet.set(dist(gen));
                    return std::abs(magnet.get()) < 100.0; // Reasonable current
            }
            return true;
        };
        
        StressTest::EnduranceTest<decltype(endurance_test_func)> endurance_test(endurance_test_func);
        
        // Run for 45 seconds
        endurance_test.run_for_duration(std::chrono::seconds(45));
        
        // Verify components are still operational
        assert(bpm.is_healthy());
        assert(bic.is_healthy());
        assert(magnet.is_healthy());
        
        assert(bpm.self_test());
        assert(bic.self_test());
        assert(magnet.self_test());
        
        std::cout << "  Final component status:" << std::endl;
        std::cout << "    BPM reads: " << bpm.get_read_count() << std::endl;
        std::cout << "    Magnet power: " << magnet.get_power() << "W" << std::endl;
        std::cout << "    Magnet energy: " << magnet.get_total_energy_dissipated() << "J" << std::endl;
        
        std::cout << "✅ Endurance test PASSED" << std::endl;
    }
    
    std::cout << "\n🎉 ALL HARDWARE COMPONENT STRESS TESTS PASSED!" << std::endl;
    std::cout << "📊 Hardware components validated for production real-time use" << std::endl;
    
    return 0;
}
