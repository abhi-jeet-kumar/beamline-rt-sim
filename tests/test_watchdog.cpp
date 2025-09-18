#include "../src/core/watchdog.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief Unit tests for Watchdog functionality
 * 
 * Tests include:
 * 1. Basic deadline detection
 * 2. Consecutive violation tracking
 * 3. Statistics collection
 * 4. Warning threshold behavior
 * 5. Reset functionality
 * 6. Callback mechanisms
 * 7. Thread safety
 */

int main() {
    std::cout << "Testing Watchdog functionality..." << std::endl;
    
    // Test 1: Basic deadline detection
    {
        std::cout << "Test 1: Basic deadline detection" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100)); // 100μs budget
        
        // Normal execution (within budget)
        auto start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        auto end = std::chrono::steady_clock::now();
        
        bool missed = wd.check(start, end);
        assert(!missed);
        assert(!wd.is_tripped());
        assert(wd.get_consecutive_misses() == 0);
        assert(wd.get_total_violations() == 0);
        assert(wd.get_total_checks() == 1);
        
        // Execution exceeding budget
        start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::microseconds(150));
        end = std::chrono::steady_clock::now();
        
        missed = wd.check(start, end);
        assert(missed);
        assert(wd.is_tripped());
        assert(wd.get_consecutive_misses() == 1);
        assert(wd.get_total_violations() == 1);
        assert(wd.get_total_checks() == 2);
        
        std::cout << "  Basic deadline detection test passed" << std::endl;
    }
    
    // Test 2: Consecutive violation tracking
    {
        std::cout << "Test 2: Consecutive violation tracking" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(50));
        
        // Multiple consecutive violations
        for (int i = 0; i < 5; i++) {
            bool missed = wd.check_duration(std::chrono::microseconds(100));
            assert(missed);
            assert(wd.get_consecutive_misses() == static_cast<uint32_t>(i + 1));
        }
        
        assert(wd.get_total_violations() == 5);
        assert(wd.is_critical()); // Should be critical after 5 consecutive misses
        
        // One successful execution should reset consecutive counter
        bool missed = wd.check_duration(std::chrono::microseconds(25));
        assert(!missed);
        assert(wd.get_consecutive_misses() == 0);
        assert(!wd.is_critical());
        assert(wd.get_total_violations() == 5); // Total count preserved
        
        std::cout << "  Consecutive violation tracking test passed" << std::endl;
    }
    
    // Test 3: Statistics collection
    {
        std::cout << "Test 3: Statistics collection" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100));
        
        // Known execution times
        std::vector<std::chrono::microseconds> execution_times = {
            std::chrono::microseconds(20),
            std::chrono::microseconds(50),
            std::chrono::microseconds(80),
            std::chrono::microseconds(120), // This one exceeds budget
            std::chrono::microseconds(30)
        };
        
        for (auto exec_time : execution_times) {
            wd.check_duration(exec_time);
        }
        
        // Check statistics
        assert(wd.get_total_checks() == 5);
        assert(wd.get_total_violations() == 1); // Only the 120μs execution
        assert(wd.get_violation_rate() == 20.0); // 1/5 = 20%
        
        double mean_exec = wd.get_mean_execution_ns();
        double expected_mean = (20 + 50 + 80 + 120 + 30) * 1000.0 / 5.0; // Convert to ns
        assert(std::abs(mean_exec - expected_mean) < 1000.0); // Within 1μs tolerance
        
        assert(wd.get_min_execution_ns() >= 20000); // At least 20μs in ns
        assert(wd.get_max_execution_ns() >= 120000); // At least 120μs in ns
        
        std::cout << "  Statistics: mean=" << (mean_exec/1000.0) << "μs, " 
                  << "min=" << (wd.get_min_execution_ns()/1000.0) << "μs, "
                  << "max=" << (wd.get_max_execution_ns()/1000.0) << "μs" << std::endl;
        
        std::cout << "  Statistics collection test passed" << std::endl;
    }
    
    // Test 4: Warning threshold behavior
    {
        std::cout << "Test 4: Warning threshold behavior" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100), 0.7); // 70μs warning threshold
        
        // Execution within warning threshold
        bool missed = wd.check_duration(std::chrono::microseconds(60));
        assert(!missed);
        assert(wd.get_consecutive_warnings() == 0);
        assert(wd.get_total_warnings() == 0);
        
        // Execution exceeding warning but not deadline
        missed = wd.check_duration(std::chrono::microseconds(80));
        assert(!missed); // No deadline miss
        assert(wd.get_consecutive_warnings() == 1);
        assert(wd.get_total_warnings() == 1);
        
        // Another warning
        missed = wd.check_duration(std::chrono::microseconds(85));
        assert(!missed);
        assert(wd.get_consecutive_warnings() == 2);
        assert(wd.get_total_warnings() == 2);
        
        // Back to normal - should reset consecutive warnings
        missed = wd.check_duration(std::chrono::microseconds(50));
        assert(!missed);
        assert(wd.get_consecutive_warnings() == 0);
        assert(wd.get_total_warnings() == 2); // Total preserved
        
        double warning_rate = wd.get_warning_rate();
        assert(std::abs(warning_rate - 50.0) < 1.0); // 2/4 = 50%
        
        std::cout << "  Warning rate: " << warning_rate << "%" << std::endl;
        std::cout << "  Warning threshold test passed" << std::endl;
    }
    
    // Test 5: Reset functionality
    {
        std::cout << "Test 5: Reset functionality" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(50));
        
        // Generate some violations and statistics
        for (int i = 0; i < 3; i++) {
            wd.check_duration(std::chrono::microseconds(100));
        }
        
        assert(wd.get_consecutive_misses() == 3);
        assert(wd.get_total_violations() == 3);
        assert(wd.is_tripped());
        
        // Reset (preserves statistics)
        wd.reset();
        assert(wd.get_consecutive_misses() == 0);
        assert(!wd.is_tripped());
        assert(wd.get_total_violations() == 3); // Statistics preserved
        
        // Reset all (clears everything)
        wd.reset_all();
        assert(wd.get_consecutive_misses() == 0);
        assert(wd.get_total_violations() == 0);
        assert(wd.get_total_checks() == 0);
        assert(!wd.is_tripped());
        
        std::cout << "  Reset functionality test passed" << std::endl;
    }
    
    // Test 6: Callback mechanisms
    {
        std::cout << "Test 6: Callback mechanisms" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100));
        wd.set_thresholds(3, 5); // 3 for critical, 5 for warning
        
        bool critical_called = false;
        bool warning_called = false;
        
        wd.set_critical_callback([&critical_called](const Watchdog& w) {
            critical_called = true;
            std::cout << "    Critical callback triggered with " 
                      << w.get_consecutive_misses() << " consecutive misses" << std::endl;
        });
        
        wd.set_warning_callback([&warning_called](const Watchdog& w) {
            warning_called = true;
            std::cout << "    Warning callback triggered with " 
                      << w.get_consecutive_warnings() << " consecutive warnings" << std::endl;
        });
        
        // Generate 2 violations (should not trigger critical)
        for (int i = 0; i < 2; i++) {
            wd.check_duration(std::chrono::microseconds(150));
        }
        assert(!critical_called);
        
        // Third violation should trigger critical callback
        wd.check_duration(std::chrono::microseconds(150));
        assert(critical_called);
        
        // Test warning callbacks with warning threshold violations
        wd.reset();
        for (int i = 0; i < 5; i++) {
            wd.check_duration(std::chrono::microseconds(90)); // Exceeds warning threshold
        }
        assert(warning_called);
        
        std::cout << "  Callback mechanisms test passed" << std::endl;
    }
    
    // Test 7: Budget adjustment
    {
        std::cout << "Test 7: Budget adjustment" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100));
        
        // Initially should miss deadline
        bool missed = wd.check_duration(std::chrono::microseconds(150));
        assert(missed);
        
        // Increase budget - same execution time should now pass
        wd.set_budget(std::chrono::microseconds(200));
        missed = wd.check_duration(std::chrono::microseconds(150));
        assert(!missed);
        
        assert(wd.get_budget() == std::chrono::microseconds(200));
        
        std::cout << "  Budget adjustment test passed" << std::endl;
    }
    
    // Test 8: Health assessment
    {
        std::cout << "Test 8: Health assessment" << std::endl;
        
        Watchdog wd(std::chrono::microseconds(100));
        
        // Initially healthy
        assert(wd.is_healthy());
        
        // Add a few violations but keep rate low
        wd.check_duration(std::chrono::microseconds(150)); // Miss
        for (int i = 0; i < 100; i++) {
            wd.check_duration(std::chrono::microseconds(50)); // Success
        }
        
        // Should still be healthy (violation rate ~1%)
        assert(wd.is_healthy());
        
        // Add many violations to exceed 1% rate
        for (int i = 0; i < 5; i++) {
            wd.check_duration(std::chrono::microseconds(150)); // Miss
        }
        
        // Should now be unhealthy due to violation rate
        assert(!wd.is_healthy());
        
        std::cout << "  Final violation rate: " << wd.get_violation_rate() << "%" << std::endl;
        std::cout << "  Health assessment test passed" << std::endl;
    }
    
    std::cout << "\n✅ All Watchdog unit tests passed!" << std::endl;
    return 0;
}
