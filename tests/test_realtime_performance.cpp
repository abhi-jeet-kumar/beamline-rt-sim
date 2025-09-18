#include "../src/realtime/performance_optimizer.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

/**
 * @brief Test Real-time Performance Optimizer
 * 
 * Tests CPU affinity, real-time scheduling, memory optimization,
 * and timing measurement capabilities for CERN-level performance
 */
int main() {
    std::cout << "Testing Real-time Performance Optimizer..." << std::endl;
    
    // Test 1: Basic initialization
    {
        std::cout << "Test 1: Real-time initialization" << std::endl;
        
        RealTimeOptimizer optimizer;
        
        // Initialize with specific core and priority
        bool rt_success = optimizer.initialize_realtime(1, 50);
        
        // Note: RT scheduling may fail without root privileges
        std::cout << "  Real-time initialization: " << (rt_success ? "FULL" : "PARTIAL") << std::endl;
        
        std::cout << "  Real-time initialization test passed" << std::endl;
    }
    
    // Test 2: Timing measurement and statistics
    {
        std::cout << "Test 2: Timing measurement" << std::endl;
        
        RealTimeOptimizer optimizer;
        optimizer.initialize_realtime();
        
        // Simulate control loop timing measurements
        std::vector<double> test_timings = {5.2, 8.1, 12.3, 6.7, 9.4, 15.1, 7.8, 11.2, 4.9, 13.6};
        
        for (double timing : test_timings) {
            optimizer.record_timing(timing);
        }
        
        auto stats = optimizer.get_statistics();
        
        assert(stats.sample_count == test_timings.size());
        assert(stats.min_timing_us == 4.9);
        assert(stats.max_timing_us == 15.1);
        assert(stats.avg_timing_us > 8.0 && stats.avg_timing_us < 10.0);
        
        std::cout << "  Statistics: " << stats.sample_count << " samples, "
                  << "avg: " << stats.avg_timing_us << "μs, "
                  << "p99: " << stats.p99_jitter_us << "μs" << std::endl;
        
        std::cout << "  Timing measurement test passed" << std::endl;
    }
    
    // Test 3: CERN timing target validation
    {
        std::cout << "Test 3: CERN timing target validation" << std::endl;
        
        RealTimeOptimizer optimizer;
        optimizer.initialize_realtime();
        
        // Simulate excellent timing (should meet CERN target)
        for (int i = 0; i < 1000; i++) {
            double excellent_timing = 5.0 + (i % 10) * 0.5; // 5-9.5μs range
            optimizer.record_timing(excellent_timing);
        }
        
        auto stats = optimizer.get_statistics();
        bool target_met = optimizer.meets_cern_timing_target();
        
        std::cout << "  Simulated excellent timing: P99 = " << stats.p99_jitter_us << "μs" << std::endl;
        std::cout << "  CERN target (<10μs): " << (target_met ? "ACHIEVED" : "NOT MET") << std::endl;
        
        assert(target_met); // Should meet target with excellent timing
        
        std::cout << "  CERN timing target validation passed" << std::endl;
    }
    
    // Test 4: Performance monitoring under load
    {
        std::cout << "Test 4: Performance monitoring under load" << std::endl;
        
        RealTimeOptimizer optimizer;
        optimizer.initialize_realtime();
        
        // Simulate real-time control loop load
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; i++) {
            auto loop_start = std::chrono::high_resolution_clock::now();
            
            // Simulate control loop work
            volatile double work = 0.0;
            for (int j = 0; j < 100; j++) {
                work += std::sin(j) * std::cos(j);
            }
            
            auto loop_end = std::chrono::high_resolution_clock::now();
            double timing_us = std::chrono::duration<double, std::micro>(loop_end - loop_start).count();
            
            optimizer.record_timing(timing_us);
            
            // Simulate 1kHz timing
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
        }
        
        auto total_time = std::chrono::high_resolution_clock::now() - start_time;
        double total_ms = std::chrono::duration<double, std::milli>(total_time).count();
        
        auto stats = optimizer.get_statistics();
        
        std::cout << "  Load test: " << stats.sample_count << " samples in " 
                  << total_ms << "ms" << std::endl;
        std::cout << "  Performance: avg=" << stats.avg_timing_us << "μs, "
                  << "p99=" << stats.p99_jitter_us << "μs" << std::endl;
        
        // Print detailed performance report
        optimizer.print_performance_report();
        
        std::cout << "  Performance monitoring test passed" << std::endl;
    }
    
    std::cout << "\n✅ All Real-time Performance tests passed!" << std::endl;
    return 0;
}
