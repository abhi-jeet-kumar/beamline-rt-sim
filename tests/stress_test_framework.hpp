#pragma once
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>

/**
 * @brief Comprehensive stress and load testing framework for real-time systems
 * 
 * Provides utilities for:
 * - High-frequency load testing
 * - Resource exhaustion stress testing  
 * - Real-time deadline analysis
 * - Long-running endurance testing
 * - Statistical performance analysis
 */

namespace StressTest {

/**
 * @brief Performance measurement collector
 */
class PerformanceMonitor {
private:
    std::vector<double> samples;
    std::chrono::steady_clock::time_point start_time;
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> deadline_misses{0};
    
public:
    PerformanceMonitor() : start_time(std::chrono::steady_clock::now()) {
        samples.reserve(100000); // Pre-allocate for performance
    }
    
    void record_timing(double time_us) {
        samples.push_back(time_us);
        total_operations++;
    }
    
    void record_deadline_miss() {
        deadline_misses++;
    }
    
    struct Stats {
        double mean_us;
        double std_dev_us;
        double min_us;
        double max_us;
        double p95_us;
        double p99_us;
        uint64_t total_ops;
        uint64_t deadline_misses;
        double deadline_miss_rate;
        double throughput_ops_per_sec;
        double duration_sec;
    };
    
    Stats get_statistics() const {
        if (samples.empty()) return {};
        
        auto now = std::chrono::steady_clock::now();
        double duration = std::chrono::duration<double>(now - start_time).count();
        
        std::vector<double> sorted_samples = samples;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        
        double sum = std::accumulate(sorted_samples.begin(), sorted_samples.end(), 0.0);
        double mean = sum / sorted_samples.size();
        
        double variance = 0.0;
        for (double sample : sorted_samples) {
            variance += std::pow(sample - mean, 2);
        }
        variance /= sorted_samples.size();
        double std_dev = std::sqrt(variance);
        
        size_t p95_idx = static_cast<size_t>(sorted_samples.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(sorted_samples.size() * 0.99);
        
        return {
            .mean_us = mean,
            .std_dev_us = std_dev,
            .min_us = sorted_samples.front(),
            .max_us = sorted_samples.back(),
            .p95_us = sorted_samples[p95_idx],
            .p99_us = sorted_samples[p99_idx],
            .total_ops = total_operations.load(),
            .deadline_misses = deadline_misses.load(),
            .deadline_miss_rate = static_cast<double>(deadline_misses.load()) / total_operations.load(),
            .throughput_ops_per_sec = total_operations.load() / duration,
            .duration_sec = duration
        };
    }
    
    void print_statistics(const std::string& test_name) const {
        auto stats = get_statistics();
        std::cout << "\n📊 " << test_name << " Performance Statistics:" << std::endl;
        std::cout << "  Duration: " << stats.duration_sec << " seconds" << std::endl;
        std::cout << "  Total Operations: " << stats.total_ops << std::endl;
        std::cout << "  Throughput: " << stats.throughput_ops_per_sec << " ops/sec" << std::endl;
        std::cout << "  Timing - Mean: " << stats.mean_us << "μs, StdDev: " << stats.std_dev_us << "μs" << std::endl;
        std::cout << "  Timing - Min: " << stats.min_us << "μs, Max: " << stats.max_us << "μs" << std::endl;
        std::cout << "  Percentiles - P95: " << stats.p95_us << "μs, P99: " << stats.p99_us << "μs" << std::endl;
        std::cout << "  Deadline Misses: " << stats.deadline_misses << " (" 
                  << (stats.deadline_miss_rate * 100) << "%)" << std::endl;
    }
};

/**
 * @brief CPU load generator for stress testing
 */
class CPUStressor {
private:
    std::atomic<bool> running{false};
    std::vector<std::thread> stress_threads;
    
public:
    void start_stress(int num_threads = std::thread::hardware_concurrency()) {
        running = true;
        
        for (int i = 0; i < num_threads; i++) {
            stress_threads.emplace_back([this]() {
                while (running.load()) {
                    // CPU-intensive work
                    volatile double result = 0.0;
                    for (int j = 0; j < 10000; j++) {
                        result += std::sin(j) * std::cos(j);
                    }
                }
            });
        }
        
        std::cout << "🔥 Started CPU stress with " << num_threads << " threads" << std::endl;
    }
    
    void stop_stress() {
        running = false;
        for (auto& thread : stress_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        stress_threads.clear();
        std::cout << "✅ Stopped CPU stress" << std::endl;
    }
    
    ~CPUStressor() {
        stop_stress();
    }
};

/**
 * @brief Memory allocator for memory pressure testing
 */
class MemoryStressor {
private:
    std::vector<std::unique_ptr<char[]>> allocations;
    
public:
    void allocate_memory_mb(size_t mb) {
        size_t bytes = mb * 1024 * 1024;
        auto ptr = std::make_unique<char[]>(bytes);
        
        // Touch all pages to ensure allocation
        for (size_t i = 0; i < bytes; i += 4096) {
            ptr[i] = static_cast<char>(i % 256);
        }
        
        allocations.push_back(std::move(ptr));
        std::cout << "🧠 Allocated " << mb << "MB (total: " 
                  << allocations.size() * mb << "MB)" << std::endl;
    }
    
    void free_all() {
        size_t total_mb = allocations.size() * 
                         (allocations.empty() ? 0 : allocations[0].get() ? 1 : 0);
        allocations.clear();
        std::cout << "✅ Freed all memory allocations" << std::endl;
    }
};

/**
 * @brief Real-time deadline testing framework
 */
template<typename TestFunction>
class RealtimeStressTest {
private:
    TestFunction test_func;
    std::chrono::nanoseconds period;
    std::chrono::nanoseconds deadline;
    
public:
    RealtimeStressTest(TestFunction func, std::chrono::nanoseconds p, 
                      std::chrono::nanoseconds d = std::chrono::nanoseconds::zero())
        : test_func(func), period(p), deadline(d.count() > 0 ? d : p) {}
    
    struct Results {
        PerformanceMonitor::Stats stats;
        bool passed;
        std::string failure_reason;
    };
    
    Results run_test(int iterations, bool with_cpu_stress = false, 
                    bool with_memory_stress = false) {
        std::cout << "\n🚀 Starting real-time stress test..." << std::endl;
        std::cout << "  Period: " << period.count() / 1000.0 << "μs" << std::endl;
        std::cout << "  Deadline: " << deadline.count() / 1000.0 << "μs" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  CPU Stress: " << (with_cpu_stress ? "YES" : "NO") << std::endl;
        std::cout << "  Memory Stress: " << (with_memory_stress ? "YES" : "NO") << std::endl;
        
        PerformanceMonitor monitor;
        CPUStressor cpu_stress;
        MemoryStressor mem_stress;
        
        // Start stress conditions
        if (with_cpu_stress) {
            cpu_stress.start_stress();
        }
        if (with_memory_stress) {
            mem_stress.allocate_memory_mb(100); // 100MB pressure
        }
        
        // Run test iterations
        auto start_time = std::chrono::steady_clock::now();
        auto next_wake = start_time + period;
        
        for (int i = 0; i < iterations; i++) {
            auto iteration_start = std::chrono::steady_clock::now();
            
            // Execute test function
            test_func();
            
            auto iteration_end = std::chrono::steady_clock::now();
            auto execution_time = iteration_end - iteration_start;
            
            // Record timing
            double exec_time_us = std::chrono::duration<double, std::micro>(execution_time).count();
            monitor.record_timing(exec_time_us);
            
            // Check deadline
            if (execution_time > deadline) {
                monitor.record_deadline_miss();
            }
            
            // Wait for next period
            std::this_thread::sleep_until(next_wake);
            next_wake += period;
        }
        
        // Clean up stress conditions
        if (with_memory_stress) {
            mem_stress.free_all();
        }
        if (with_cpu_stress) {
            cpu_stress.stop_stress();
        }
        
        auto stats = monitor.get_statistics();
        monitor.print_statistics("Real-time Stress Test");
        
        // Determine pass/fail
        Results results;
        results.stats = stats;
        results.passed = true;
        
        // Check failure conditions
        if (stats.deadline_miss_rate > 0.01) { // > 1% deadline misses
            results.passed = false;
            results.failure_reason += "Deadline miss rate too high (" + 
                                    std::to_string(stats.deadline_miss_rate * 100) + "%). ";
        }
        
        if (stats.p99_us > deadline.count() / 1000.0) { // P99 exceeds deadline
            results.passed = false;
            results.failure_reason += "P99 latency exceeds deadline. ";
        }
        
        if (stats.std_dev_us > (deadline.count() / 1000.0) * 0.1) { // High jitter
            results.passed = false;
            results.failure_reason += "Timing jitter too high. ";
        }
        
        std::cout << "\n🏁 Test Result: " << (results.passed ? "✅ PASS" : "❌ FAIL") << std::endl;
        if (!results.passed) {
            std::cout << "   Reason: " << results.failure_reason << std::endl;
        }
        
        return results;
    }
};

/**
 * @brief Endurance test runner for long-term stability
 */
template<typename TestFunction>
class EnduranceTest {
private:
    TestFunction test_func;
    std::atomic<bool> should_stop{false};
    
public:
    EnduranceTest(TestFunction func) : test_func(func) {}
    
    void run_for_duration(std::chrono::seconds duration) {
        std::cout << "\n⏰ Starting " << duration.count() << "s endurance test..." << std::endl;
        
        PerformanceMonitor monitor;
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + duration;
        
        uint64_t iteration = 0;
        while (std::chrono::steady_clock::now() < end_time && !should_stop.load()) {
            auto iter_start = std::chrono::steady_clock::now();
            
            test_func();
            
            auto iter_end = std::chrono::steady_clock::now();
            double exec_time_us = std::chrono::duration<double, std::micro>(iter_end - iter_start).count();
            monitor.record_timing(exec_time_us);
            
            iteration++;
            
            // Report progress every 10 seconds
            if (iteration % 10000 == 0) {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                double progress = std::chrono::duration<double>(elapsed).count() / duration.count() * 100;
                std::cout << "  Progress: " << std::fixed << std::setprecision(1) 
                          << progress << "% (" << iteration << " iterations)" << std::endl;
            }
            
            // Small delay to prevent CPU overload
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        monitor.print_statistics("Endurance Test");
    }
    
    void stop() {
        should_stop = true;
    }
};

} // namespace StressTest
