#include "../src/core/clock.hpp"
#include <chrono>
#include <vector>
#include <cassert>
#include <cmath>
#include <iostream>
#include <algorithm>

/**
 * @brief Test basic PeriodicClock functionality and timing accuracy
 * 
 * This test verifies:
 * 1. Clock period consistency
 * 2. Timing accuracy within acceptable bounds
 * 3. Low jitter for real-time requirements
 * 4. Period change functionality
 */
int main() {
    using namespace std::chrono;
    
    std::cout << "Testing PeriodicClock timing accuracy..." << std::endl;
    
    // Test 1: Basic timing accuracy at 1kHz (1ms period)
    {
        std::cout << "Test 1: 1kHz timing accuracy" << std::endl;
        
        PeriodicClock clk(microseconds(1000)); // 1 kHz
        auto start_time = steady_clock::now();
        
        std::vector<long long> deltas;
        auto prev = steady_clock::now();
        
        const int num_iterations = 1000;
        for (int i = 0; i < num_iterations; i++) {
            clk.wait_next();
            auto now = steady_clock::now();
            deltas.push_back(duration_cast<microseconds>(now - prev).count());
            prev = now;
        }
        
        auto total_time = steady_clock::now() - start_time;
        auto expected_total = microseconds(num_iterations * 1000);
        auto actual_total = duration_cast<microseconds>(total_time);
        
        // Check overall timing accuracy (should be within 1% of expected)
        double timing_error = std::abs(actual_total.count() - expected_total.count()) / 
                              static_cast<double>(expected_total.count());
        
        std::cout << "  Expected total time: " << expected_total.count() << " μs" << std::endl;
        std::cout << "  Actual total time: " << actual_total.count() << " μs" << std::endl;
        std::cout << "  Timing error: " << (timing_error * 100) << "%" << std::endl;
        
        assert(timing_error < 0.01); // Less than 1% error
        
        // Check individual period accuracy
        int within_tolerance = 0;
        const long long tolerance_us = 100; // 100 μs tolerance
        
        for (auto delta : deltas) {
            if (std::abs(delta - 1000) < tolerance_us) {
                within_tolerance++;
            }
        }
        
        double accuracy = static_cast<double>(within_tolerance) / deltas.size();
        std::cout << "  Periods within ±" << tolerance_us << "μs: " 
                  << (accuracy * 100) << "%" << std::endl;
        
        assert(accuracy > 0.90); // At least 90% of periods within tolerance
        
        // Check jitter (standard deviation)
        double mean = 1000.0; // Expected 1000 μs
        double variance = 0.0;
        for (auto delta : deltas) {
            variance += std::pow(delta - mean, 2);
        }
        variance /= deltas.size();
        double jitter = std::sqrt(variance);
        
        std::cout << "  Jitter (std dev): " << jitter << " μs" << std::endl;
        assert(jitter < 50.0); // Less than 50 μs jitter
    }
    
    // Test 2: Period change functionality
    {
        std::cout << "\nTest 2: Period change functionality" << std::endl;
        
        PeriodicClock clk(microseconds(500)); // Start at 2 kHz
        
        // Initial period check
        assert(clk.get_period() == microseconds(500));
        
        // Change to 1 kHz
        clk.set_period(microseconds(1000));
        assert(clk.get_period() == microseconds(1000));
        
        // Test a few cycles with new period
        auto start = steady_clock::now();
        for (int i = 0; i < 5; i++) {
            clk.wait_next();
        }
        auto elapsed = steady_clock::now() - start;
        auto elapsed_us = duration_cast<microseconds>(elapsed).count();
        
        // Should be approximately 5ms (5 * 1000μs)
        assert(std::abs(elapsed_us - 5000) < 500); // Within 500μs tolerance
        
        std::cout << "  Period change test passed" << std::endl;
    }
    
    // Test 3: Time-to-next functionality
    {
        std::cout << "\nTest 3: Time-to-next functionality" << std::endl;
        
        PeriodicClock clk(microseconds(1000));
        
        // Right after creation, should have close to full period remaining
        auto time_to_next = clk.time_to_next();
        assert(time_to_next > microseconds(900)); // Should be close to 1000μs
        assert(time_to_next <= microseconds(1000));
        
        // Wait a bit and check again
        std::this_thread::sleep_for(microseconds(200));
        time_to_next = clk.time_to_next();
        assert(time_to_next < microseconds(900)); // Should be less now
        
        std::cout << "  Time-to-next test passed" << std::endl;
    }
    
    // Test 4: High frequency test (10 kHz for short duration)
    {
        std::cout << "\nTest 4: High frequency (10kHz) short test" << std::endl;
        
        PeriodicClock clk(microseconds(100)); // 10 kHz
        
        std::vector<long long> deltas;
        auto prev = steady_clock::now();
        
        const int num_iterations = 100; // Short test to avoid long run time
        for (int i = 0; i < num_iterations; i++) {
            clk.wait_next();
            auto now = steady_clock::now();
            deltas.push_back(duration_cast<microseconds>(now - prev).count());
            prev = now;
        }
        
        // Check that most periods are close to 100μs
        int within_tolerance = 0;
        for (auto delta : deltas) {
            if (std::abs(delta - 100) < 20) { // ±20μs tolerance
                within_tolerance++;
            }
        }
        
        double accuracy = static_cast<double>(within_tolerance) / deltas.size();
        std::cout << "  High frequency accuracy: " << (accuracy * 100) << "%" << std::endl;
        
        // More lenient for high frequency
        assert(accuracy > 0.80); // At least 80% accuracy at high frequency
    }
    
    std::cout << "\n✅ All PeriodicClock tests passed!" << std::endl;
    return 0;
}
