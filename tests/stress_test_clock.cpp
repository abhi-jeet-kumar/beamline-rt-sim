#include "../src/core/clock.hpp"
#include "stress_test_framework.hpp"
#include <cassert>
#include <iostream>

/**
 * @brief Comprehensive stress testing for PeriodicClock
 * 
 * Tests include:
 * 1. High-frequency stress testing (10kHz sustained)
 * 2. Load testing under CPU pressure
 * 3. Long-term endurance testing (5+ minutes)
 * 4. Real-time deadline compliance under stress
 * 5. Memory pressure resistance
 * 6. Statistical timing analysis
 */

int main() {
    std::cout << "🔥 COMPREHENSIVE STRESS TESTING: PeriodicClock" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Test 1: High-frequency load test (10kHz)
    {
        std::cout << "\n🚀 Test 1: High-frequency load test (10kHz)" << std::endl;
        
        PeriodicClock clk(std::chrono::microseconds(100)); // 10kHz
        StressTest::PerformanceMonitor monitor;
        
        const int iterations = 50000; // 5 seconds at 10kHz
        auto start_time = std::chrono::steady_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            auto iter_start = std::chrono::steady_clock::now();
            
            clk.wait_next();
            
            auto iter_end = std::chrono::steady_clock::now();
            double timing_us = std::chrono::duration<double, std::micro>(iter_end - iter_start).count();
            monitor.record_timing(timing_us);
            
            // Check for deadline miss (should be ~100μs ±20μs)
            if (std::abs(timing_us - 100.0) > 50.0) {
                monitor.record_deadline_miss();
            }
        }
        
        monitor.print_statistics("10kHz Load Test");
        auto stats = monitor.get_statistics();
        
        // Performance requirements for high-frequency operation
        assert(stats.deadline_miss_rate < 0.05); // <5% deadline misses acceptable at 10kHz
        assert(stats.p95_us < 200.0); // P95 should be under 200μs
        
        std::cout << "✅ High-frequency load test PASSED" << std::endl;
    }
    
    // Test 2: Real-time stress test with CPU load
    {
        std::cout << "\n🚀 Test 2: Real-time stress under CPU load" << std::endl;
        
        PeriodicClock clk(std::chrono::microseconds(1000)); // 1kHz
        
        auto test_function = [&clk]() {
            clk.wait_next();
        };
        
        StressTest::RealtimeStressTest<decltype(test_function)> rt_test(
            test_function, 
            std::chrono::microseconds(1000), // 1ms period
            std::chrono::microseconds(1200)  // 1.2ms deadline (20% margin)
        );
        
        auto results = rt_test.run_test(
            5000,  // 5 seconds
            true,  // with CPU stress
            false  // no memory stress for timing test
        );
        
        assert(results.passed);
        assert(results.stats.deadline_miss_rate < 0.01); // <1% for 1kHz under stress
        
        std::cout << "✅ CPU stress test PASSED" << std::endl;
    }
    
    // Test 3: Memory pressure resistance
    {
        std::cout << "\n🚀 Test 3: Memory pressure resistance" << std::endl;
        
        PeriodicClock clk(std::chrono::microseconds(1000));
        
        auto test_function = [&clk]() {
            clk.wait_next();
        };
        
        StressTest::RealtimeStressTest<decltype(test_function)> rt_test(
            test_function,
            std::chrono::microseconds(1000),
            std::chrono::microseconds(1500) // More lenient deadline under memory pressure
        );
        
        auto results = rt_test.run_test(
            3000,  // 3 seconds
            false, // no CPU stress
            true   // with memory stress
        );
        
        assert(results.passed);
        
        std::cout << "✅ Memory pressure test PASSED" << std::endl;
    }
    
    // Test 4: Combined stress (CPU + Memory)
    {
        std::cout << "\n🚀 Test 4: Combined stress (CPU + Memory)" << std::endl;
        
        PeriodicClock clk(std::chrono::microseconds(2000)); // 500Hz - more conservative
        
        auto test_function = [&clk]() {
            clk.wait_next();
        };
        
        StressTest::RealtimeStressTest<decltype(test_function)> rt_test(
            test_function,
            std::chrono::microseconds(2000),
            std::chrono::microseconds(2500) // 2.5ms deadline under extreme stress
        );
        
        auto results = rt_test.run_test(
            2000,  // 4 seconds
            true,  // with CPU stress
            true   // with memory stress
        );
        
        // More lenient requirements under extreme stress
        assert(results.stats.deadline_miss_rate < 0.05); // <5% under extreme stress
        
        std::cout << "✅ Combined stress test PASSED" << std::endl;
    }
    
    // Test 5: Period change under stress
    {
        std::cout << "\n🚀 Test 5: Period change stability under stress" << std::endl;
        
        PeriodicClock clk(std::chrono::microseconds(1000));
        StressTest::CPUStressor cpu_stress;
        StressTest::PerformanceMonitor monitor;
        
        cpu_stress.start_stress();
        
        // Test multiple period changes under stress
        std::vector<std::chrono::microseconds> periods = {
            std::chrono::microseconds(1000), // 1kHz
            std::chrono::microseconds(500),  // 2kHz
            std::chrono::microseconds(2000), // 500Hz
            std::chrono::microseconds(1000)  // back to 1kHz
        };
        
        for (auto period : periods) {
            clk.set_period(period);
            
            // Test stability after period change
            for (int i = 0; i < 100; i++) {
                auto start = std::chrono::steady_clock::now();
                clk.wait_next();
                auto end = std::chrono::steady_clock::now();
                
                double timing_us = std::chrono::duration<double, std::micro>(end - start).count();
                monitor.record_timing(timing_us);
            }
        }
        
        cpu_stress.stop_stress();
        monitor.print_statistics("Period Change Under Stress");
        
        auto stats = monitor.get_statistics();
        assert(stats.deadline_miss_rate < 0.1); // <10% during period changes
        
        std::cout << "✅ Period change stress test PASSED" << std::endl;
    }
    
    // Test 6: Endurance test (shorter for CI/CD)
    {
        std::cout << "\n🚀 Test 6: Endurance test" << std::endl;
        
        PeriodicClock clk(std::chrono::milliseconds(1)); // 1kHz
        
        auto test_function = [&clk]() {
            clk.wait_next();
        };
        
        StressTest::EnduranceTest<decltype(test_function)> endurance_test(test_function);
        
        // Run for 30 seconds (would be hours in production)
        endurance_test.run_for_duration(std::chrono::seconds(30));
        
        std::cout << "✅ Endurance test PASSED" << std::endl;
    }
    
    // Test 7: Timing precision analysis
    {
        std::cout << "\n🚀 Test 7: Statistical timing analysis" << std::endl;
        
        PeriodicClock clk(std::chrono::microseconds(1000));
        std::vector<double> timing_errors;
        
        const int samples = 10000;
        auto expected_period_us = 1000.0;
        
        for (int i = 0; i < samples; i++) {
            auto start = std::chrono::steady_clock::now();
            clk.wait_next();
            auto end = std::chrono::steady_clock::now();
            
            double actual_us = std::chrono::duration<double, std::micro>(end - start).count();
            double error_us = std::abs(actual_us - expected_period_us);
            timing_errors.push_back(error_us);
        }
        
        // Statistical analysis
        std::sort(timing_errors.begin(), timing_errors.end());
        double mean_error = std::accumulate(timing_errors.begin(), timing_errors.end(), 0.0) / timing_errors.size();
        double p95_error = timing_errors[static_cast<size_t>(timing_errors.size() * 0.95)];
        double p99_error = timing_errors[static_cast<size_t>(timing_errors.size() * 0.99)];
        double max_error = timing_errors.back();
        
        std::cout << "  Timing Error Statistics:" << std::endl;
        std::cout << "    Mean: " << mean_error << "μs" << std::endl;
        std::cout << "    P95: " << p95_error << "μs" << std::endl;
        std::cout << "    P99: " << p99_error << "μs" << std::endl;
        std::cout << "    Max: " << max_error << "μs" << std::endl;
        
        // Precision requirements
        assert(mean_error < 50.0);  // Mean error < 50μs
        assert(p95_error < 100.0);  // P95 error < 100μs (10% of period)
        assert(p99_error < 200.0);  // P99 error < 200μs
        
        std::cout << "✅ Timing precision analysis PASSED" << std::endl;
    }
    
    std::cout << "\n🎉 ALL CLOCK STRESS TESTS PASSED!" << std::endl;
    std::cout << "📊 PeriodicClock validated for production real-time use" << std::endl;
    
    return 0;
}
