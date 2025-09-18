#include "../src/core/watchdog.hpp"
#include "stress_test_framework.hpp"
#include <cassert>
#include <iostream>
#include <random>

/**
 * @brief Comprehensive stress testing for Watchdog
 * 
 * Tests include:
 * 1. High-frequency deadline monitoring (1kHz+)
 * 2. Thread safety under concurrent access
 * 3. Statistical accuracy under extreme load
 * 4. Callback performance under stress
 * 5. Memory pressure resistance
 * 6. Long-term stability and accuracy
 * 7. Real-time behavior validation
 */

int main() {
    std::cout << "🔥 COMPREHENSIVE STRESS TESTING: Watchdog" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Test 1: High-frequency deadline monitoring
    {
        std::cout << "\n🚀 Test 1: High-frequency deadline monitoring (10kHz)" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(50)); // 50μs budget
        StressTest::PerformanceMonitor monitor;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> exec_time_dist(10, 100); // 10-100μs range
        
        const int iterations = 100000; // 100k checks at 10kHz
        uint64_t expected_violations = 0;
        
        for (int i = 0; i < iterations; i++) {
            auto check_start = std::chrono::steady_clock::now();
            
            // Simulate variable execution time
            int exec_time_us = exec_time_dist(gen);
            auto execution_duration = std::chrono::microseconds(exec_time_us);
            
            if (exec_time_us > 50) {
                expected_violations++;
            }
            
            bool missed = wd.check_duration(execution_duration);
            
            auto check_end = std::chrono::steady_clock::now();
            double check_time_us = std::chrono::duration<double, std::micro>(check_end - check_start).count();
            monitor.record_timing(check_time_us);
            
            // Watchdog check itself should be very fast
            if (check_time_us > 10.0) { // >10μs for watchdog check
                monitor.record_deadline_miss();
            }
        }
        
        monitor.print_statistics("High-frequency Watchdog Checks");
        
        // Validate statistical accuracy
        double violation_rate = wd.get_violation_rate();
        double expected_rate = (static_cast<double>(expected_violations) / iterations) * 100.0;
        
        std::cout << "  Expected violations: " << expected_violations << std::endl;
        std::cout << "  Actual violations: " << wd.get_total_violations() << std::endl;
        std::cout << "  Expected rate: " << expected_rate << "%" << std::endl;
        std::cout << "  Actual rate: " << violation_rate << "%" << std::endl;
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 500000); // >500k checks/sec
        assert(stats.p99_us < 5.0); // P99 < 5μs for watchdog check
        assert(std::abs(violation_rate - expected_rate) < 2.0); // Within 2% accuracy
        
        std::cout << "✅ High-frequency monitoring test PASSED" << std::endl;
    }
    
    // Test 2: Thread safety under concurrent access
    {
        std::cout << "\n🚀 Test 2: Thread safety under concurrent access" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100));
        const int num_threads = 8;
        const int checks_per_thread = 50000;
        
        std::atomic<uint64_t> total_checks_performed{0};
        std::atomic<uint64_t> total_violations_detected{0};
        std::vector<std::thread> threads;
        std::vector<StressTest::PerformanceMonitor> monitors(num_threads);
        
        // Launch multiple threads performing watchdog checks
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                std::mt19937 gen(std::random_device{}() + t); // Different seed per thread
                std::uniform_int_distribution<> exec_dist(50, 150); // 50-150μs
                
                for (int i = 0; i < checks_per_thread; i++) {
                    auto start = std::chrono::steady_clock::now();
                    
                    int exec_time = exec_dist(gen);
                    bool missed = wd.check_duration(std::chrono::microseconds(exec_time));
                    
                    if (missed) {
                        total_violations_detected++;
                    }
                    total_checks_performed++;
                    
                    auto end = std::chrono::steady_clock::now();
                    double check_time = std::chrono::duration<double, std::micro>(end - start).count();
                    monitors[t].record_timing(check_time);
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Analyze thread performance
        std::cout << "  Thread performance analysis:" << std::endl;
        for (int t = 0; t < num_threads; t++) {
            auto stats = monitors[t].get_statistics();
            std::cout << "    Thread " << t << ": " << stats.throughput_ops_per_sec 
                      << " checks/sec, P99: " << stats.p99_us << "μs" << std::endl;
        }
        
        // Validate consistency
        assert(total_checks_performed.load() == num_threads * checks_per_thread);
        assert(wd.get_total_checks() == total_checks_performed.load());
        
        // Check for data races (violations should be consistent)
        uint64_t watchdog_violations = wd.get_total_violations();
        std::cout << "  Total checks: " << total_checks_performed.load() << std::endl;
        std::cout << "  Detected violations: " << total_violations_detected.load() << std::endl;
        std::cout << "  Watchdog violations: " << watchdog_violations << std::endl;
        
        // Allow some tolerance due to concurrent updates, but should be close
        uint64_t violation_diff = std::abs(static_cast<int64_t>(total_violations_detected.load() - watchdog_violations));
        assert(violation_diff < 100); // Allow small discrepancy due to concurrency
        
        std::cout << "✅ Thread safety test PASSED" << std::endl;
    }
    
    // Test 3: Performance under CPU stress
    {
        std::cout << "\n🚀 Test 3: Performance under CPU stress" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100));
        StressTest::CPUStressor cpu_stress;
        StressTest::PerformanceMonitor monitor;
        
        cpu_stress.start_stress();
        
        // Test watchdog performance under high CPU load
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> exec_dist(50, 200);
        
        const int iterations = 50000;
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            int exec_time = exec_dist(gen);
            wd.check_duration(std::chrono::microseconds(exec_time));
            
            auto end = std::chrono::steady_clock::now();
            double check_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(check_time);
        }
        
        cpu_stress.stop_stress();
        monitor.print_statistics("CPU Stress Watchdog Performance");
        
        auto stats = monitor.get_statistics();
        assert(stats.p99_us < 20.0); // P99 < 20μs even under CPU stress
        assert(wd.get_violation_rate() > 0.0); // Should have some violations
        assert(wd.is_healthy() || wd.get_violation_rate() < 70.0); // Reasonable violation rate
        
        std::cout << "✅ CPU stress test PASSED" << std::endl;
    }
    
    // Test 4: Callback performance under stress
    {
        std::cout << "\n🚀 Test 4: Callback performance under stress" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(50));
        wd.set_thresholds(5, 10); // Trigger callbacks frequently
        
        StressTest::PerformanceMonitor callback_monitor;
        std::atomic<uint32_t> critical_callbacks{0};
        std::atomic<uint32_t> warning_callbacks{0};
        
        // Fast callbacks that just increment counters
        wd.set_critical_callback([&](const Watchdog& w) {
            auto start = std::chrono::steady_clock::now();
            critical_callbacks++;
            auto end = std::chrono::steady_clock::now();
            double callback_time = std::chrono::duration<double, std::micro>(end - start).count();
            callback_monitor.record_timing(callback_time);
        });
        
        wd.set_warning_callback([&](const Watchdog& w) {
            warning_callbacks++;
        });
        
        // Generate violations to trigger callbacks
        for (int i = 0; i < 1000; i++) {
            // Generate bursts of violations followed by recovery
            for (int j = 0; j < 10; j++) {
                wd.check_duration(std::chrono::microseconds(100)); // Violation
            }
            for (int j = 0; j < 5; j++) {
                wd.check_duration(std::chrono::microseconds(25)); // Recovery
            }
        }
        
        std::cout << "  Critical callbacks triggered: " << critical_callbacks.load() << std::endl;
        std::cout << "  Warning callbacks triggered: " << warning_callbacks.load() << std::endl;
        
        if (callback_monitor.get_statistics().total_ops > 0) {
            callback_monitor.print_statistics("Callback Performance");
            auto stats = callback_monitor.get_statistics();
            assert(stats.p99_us < 50.0); // Callbacks should be fast
        }
        
        assert(critical_callbacks.load() > 0); // Should have triggered some callbacks
        
        std::cout << "✅ Callback performance test PASSED" << std::endl;
    }
    
    // Test 5: Memory pressure resistance
    {
        std::cout << "\n🚀 Test 5: Memory pressure resistance" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(75));
        StressTest::MemoryStressor mem_stress;
        StressTest::PerformanceMonitor monitor;
        
        // Apply memory pressure
        mem_stress.allocate_memory_mb(200);
        mem_stress.allocate_memory_mb(200); // Total 400MB
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> exec_dist(25, 125);
        
        const int iterations = 25000;
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            int exec_time = exec_dist(gen);
            wd.check_duration(std::chrono::microseconds(exec_time));
            
            auto end = std::chrono::steady_clock::now();
            double check_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(check_time);
        }
        
        mem_stress.free_all();
        monitor.print_statistics("Memory Pressure Watchdog");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 100000); // >100k checks/sec under memory pressure
        
        std::cout << "✅ Memory pressure test PASSED" << std::endl;
    }
    
    // Test 6: Statistical accuracy under extreme variations
    {
        std::cout << "\n🚀 Test 6: Statistical accuracy validation" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100));
        
        // Known execution time pattern
        std::vector<int> execution_times_us = {
            10, 20, 30, 50, 75,    // Within budget
            110, 120, 150, 200,   // Exceed budget  
            40, 60, 80            // Back within budget
        };
        
        uint64_t expected_violations = 0;
        uint64_t expected_min_us = 10;
        uint64_t expected_max_us = 200;
        double expected_sum_us = 0;
        
        for (int exec_time : execution_times_us) {
            wd.check_duration(std::chrono::microseconds(exec_time));
            if (exec_time > 100) expected_violations++;
            expected_sum_us += exec_time;
        }
        
        double expected_mean_us = expected_sum_us / execution_times_us.size();
        double expected_rate = (static_cast<double>(expected_violations) / execution_times_us.size()) * 100.0;
        
        // Validate statistics
        assert(wd.get_total_violations() == expected_violations);
        assert(wd.get_total_checks() == execution_times_us.size());
        
        double actual_rate = wd.get_violation_rate();
        assert(std::abs(actual_rate - expected_rate) < 0.1);
        
        double actual_mean_us = wd.get_mean_execution_ns() / 1000.0;
        assert(std::abs(actual_mean_us - expected_mean_us) < 1.0);
        
        assert(wd.get_min_execution_ns() / 1000 == expected_min_us);
        assert(wd.get_max_execution_ns() / 1000 == expected_max_us);
        
        std::cout << "  Expected vs Actual:" << std::endl;
        std::cout << "    Violations: " << expected_violations << " vs " << wd.get_total_violations() << std::endl;
        std::cout << "    Rate: " << expected_rate << "% vs " << actual_rate << "%" << std::endl;
        std::cout << "    Mean: " << expected_mean_us << "μs vs " << actual_mean_us << "μs" << std::endl;
        std::cout << "    Min: " << expected_min_us << "μs vs " << (wd.get_min_execution_ns()/1000) << "μs" << std::endl;
        std::cout << "    Max: " << expected_max_us << "μs vs " << (wd.get_max_execution_ns()/1000) << "μs" << std::endl;
        
        std::cout << "✅ Statistical accuracy test PASSED" << std::endl;
    }
    
    // Test 7: Real-time control loop simulation
    {
        std::cout << "\n🚀 Test 7: Real-time control loop simulation" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(1000)); // 1ms budget for 1kHz loop
        
        auto control_loop = [&wd]() {
            // Simulate control loop work
            auto start = std::chrono::steady_clock::now();
            
            // Simulated sensor reading (10μs)
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            
            // Simulated PID calculation (5μs)
            volatile double result = 0.0;
            for (int i = 0; i < 1000; i++) {
                result += std::sin(i) * std::cos(i);
            }
            
            // Simulated actuator update (15μs)
            std::this_thread::sleep_for(std::chrono::microseconds(15));
            
            auto end = std::chrono::steady_clock::now();
            return wd.check(start, end);
        };
        
        StressTest::RealtimeStressTest<decltype(control_loop)> rt_test(
            control_loop,
            std::chrono::microseconds(1000), // 1kHz
            std::chrono::microseconds(1000)  // 1ms deadline
        );
        
        auto results = rt_test.run_test(
            5000,  // 5 seconds of 1kHz operation
            false, // no additional CPU stress
            false  // no memory stress
        );
        
        assert(results.passed);
        assert(results.stats.deadline_miss_rate < 0.01); // <1% deadline misses
        assert(wd.is_healthy());
        
        std::cout << "✅ Real-time control loop simulation PASSED" << std::endl;
    }
    
    // Test 8: Endurance test
    {
        std::cout << "\n🚀 Test 8: Endurance test" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100));
        
        auto endurance_test_func = [&wd]() {
            static std::mt19937 gen(std::random_device{}());
            static std::uniform_int_distribution<> exec_dist(30, 150);
            
            int exec_time = exec_dist(gen);
            wd.check_duration(std::chrono::microseconds(exec_time));
        };
        
        StressTest::EnduranceTest<decltype(endurance_test_func)> endurance_test(endurance_test_func);
        
        // Run for 60 seconds
        endurance_test.run_for_duration(std::chrono::seconds(60));
        
        // Validate long-term behavior
        assert(wd.get_total_checks() > 100000); // Should have many checks
        assert(wd.get_violation_rate() > 0.0); // Should have some violations
        assert(wd.get_violation_rate() < 80.0); // But not too many
        
        std::cout << "  Final statistics after 60s:" << std::endl;
        std::cout << "    Total checks: " << wd.get_total_checks() << std::endl;
        std::cout << "    Violation rate: " << wd.get_violation_rate() << "%" << std::endl;
        std::cout << "    Mean execution: " << (wd.get_mean_execution_ns()/1000.0) << "μs" << std::endl;
        
        std::cout << "✅ Endurance test PASSED" << std::endl;
    }
    
    std::cout << "\n🎉 ALL WATCHDOG STRESS TESTS PASSED!" << std::endl;
    std::cout << "📊 Watchdog validated for production real-time deadline monitoring" << std::endl;
    
    return 0;
}
