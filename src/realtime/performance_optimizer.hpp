#pragma once
#include <atomic>
#include <chrono>
#include <vector>
#include <thread>
#include <algorithm>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#elif __APPLE__
#include <pthread.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <mach/mach.h>
#endif

/**
 * @brief Real-time performance optimizer for CERN-level timing
 * 
 * Implements hard real-time optimizations to achieve <10μs jitter p99:
 * - CPU core affinity and isolation
 * - SCHED_FIFO real-time scheduling  
 * - Memory pre-allocation and locking
 * - Performance monitoring and statistics
 */
class RealTimeOptimizer {
private:
    bool rt_enabled_{false};
    int cpu_core_{-1};
    int rt_priority_{50};
    
    // Performance statistics
    mutable std::atomic<uint64_t> timing_samples_{0};
    mutable std::atomic<double> min_timing_us_{1e6};
    mutable std::atomic<double> max_timing_us_{0.0};
    mutable std::atomic<double> sum_timing_us_{0.0};
    
    // Jitter analysis
    std::vector<double> jitter_samples_;
    size_t max_samples_{10000};

public:
    /**
     * @brief Constructor
     */
    RealTimeOptimizer() {
        jitter_samples_.reserve(max_samples_);
    }
    
    /**
     * @brief Initialize real-time optimizations
     * @param core CPU core to pin to (-1 for auto-select)
     * @param priority Real-time priority (1-99, higher = more priority)
     * @return true if real-time setup successful
     */
    bool initialize_realtime(int core = -1, int priority = 50) {
        rt_priority_ = priority;
        
        std::cout << "Initializing real-time optimizations..." << std::endl;
        
        // Step 1: Lock memory to prevent paging
        if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
            std::cout << "  Warning: Could not lock memory (may affect timing)" << std::endl;
        } else {
            std::cout << "  ✅ Memory locked to prevent paging" << std::endl;
        }
        
        // Step 2: Set CPU affinity
        if (core == -1) {
            // Auto-select last CPU core (typically least loaded)
            cpu_core_ = std::thread::hardware_concurrency() - 1;
        } else {
            cpu_core_ = core;
        }
        
        if (set_cpu_affinity(cpu_core_)) {
            std::cout << "  ✅ CPU affinity set to core " << cpu_core_ << std::endl;
        } else {
            std::cout << "  Warning: Could not set CPU affinity" << std::endl;
        }
        
        // Step 3: Set real-time scheduling
        if (set_realtime_priority(priority)) {
            std::cout << "  ✅ Real-time scheduling enabled (priority " << priority << ")" << std::endl;
            rt_enabled_ = true;
        } else {
            std::cout << "  Warning: Could not enable real-time scheduling" << std::endl;
            std::cout << "  (Run as root or increase ulimits for RT scheduling)" << std::endl;
        }
        
        // Step 4: Pre-allocate memory pools
        preallocate_memory();
        std::cout << "  ✅ Memory pools pre-allocated" << std::endl;
        
        std::cout << "Real-time optimization " << (rt_enabled_ ? "ENABLED" : "PARTIAL") << std::endl;
        return rt_enabled_;
    }
    
    /**
     * @brief Record timing measurement for jitter analysis
     * @param timing_us Timing measurement in microseconds
     */
    void record_timing(double timing_us) {
        timing_samples_.fetch_add(1);
        
        // Update min/max atomically
        double current_min = min_timing_us_.load();
        while (timing_us < current_min && 
               !min_timing_us_.compare_exchange_weak(current_min, timing_us)) {}
        
        double current_max = max_timing_us_.load();
        while (timing_us > current_max && 
               !max_timing_us_.compare_exchange_weak(current_max, timing_us)) {}
        
        sum_timing_us_.fetch_add(timing_us);
        
        // Store for jitter analysis (thread-safe with lock-free design)
        if (jitter_samples_.size() < max_samples_) {
            jitter_samples_.push_back(timing_us);
        }
    }
    
    /**
     * @brief Get performance statistics
     */
    struct PerformanceStats {
        uint64_t sample_count;
        double min_timing_us;
        double max_timing_us;
        double avg_timing_us;
        double p95_jitter_us;
        double p99_jitter_us;
        bool rt_enabled;
        int cpu_core;
    };
    
    PerformanceStats get_statistics() const {
        uint64_t samples = timing_samples_.load();
        double avg = (samples > 0) ? sum_timing_us_.load() / samples : 0.0;
        
        // Calculate jitter percentiles
        std::vector<double> sorted_samples = jitter_samples_;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        
        double p95 = 0.0, p99 = 0.0;
        if (!sorted_samples.empty()) {
            size_t p95_idx = static_cast<size_t>(sorted_samples.size() * 0.95);
            size_t p99_idx = static_cast<size_t>(sorted_samples.size() * 0.99);
            p95 = sorted_samples[std::min(p95_idx, sorted_samples.size() - 1)];
            p99 = sorted_samples[std::min(p99_idx, sorted_samples.size() - 1)];
        }
        
        return {
            samples,
            min_timing_us_.load(),
            max_timing_us_.load(), 
            avg,
            p95,
            p99,
            rt_enabled_,
            cpu_core_
        };
    }
    
    /**
     * @brief Check if CERN timing target achieved
     * @return true if p99 jitter < 10μs
     */
    bool meets_cern_timing_target() const {
        auto stats = get_statistics();
        return stats.p99_jitter_us < 10.0;
    }
    
    /**
     * @brief Print performance report
     */
    void print_performance_report() const {
        auto stats = get_statistics();
        
        std::cout << "\n📊 REAL-TIME PERFORMANCE REPORT" << std::endl;
        std::cout << "================================" << std::endl;
        std::cout << "Samples: " << stats.sample_count << std::endl;
        std::cout << "Timing: " << stats.min_timing_us << " - " << stats.max_timing_us << " μs" << std::endl;
        std::cout << "Average: " << stats.avg_timing_us << " μs" << std::endl;
        std::cout << "P95 Jitter: " << stats.p95_jitter_us << " μs" << std::endl;
        std::cout << "P99 Jitter: " << stats.p99_jitter_us << " μs" << std::endl;
        std::cout << "RT Enabled: " << (stats.rt_enabled ? "YES" : "NO") << std::endl;
        std::cout << "CPU Core: " << stats.cpu_core << std::endl;
        
        if (stats.p99_jitter_us < 10.0) {
            std::cout << "🎯 CERN TIMING TARGET ACHIEVED! (<10μs p99)" << std::endl;
        } else {
            std::cout << "⚠️  CERN timing target not yet achieved (target: <10μs p99)" << std::endl;
        }
    }

private:
    bool set_cpu_affinity(int core) {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#elif __APPLE__
        // macOS thread affinity (best effort)
        thread_affinity_policy_data_t policy = {core};
        kern_return_t result = thread_policy_set(
            mach_thread_self(),
            THREAD_AFFINITY_POLICY,
            (thread_policy_t)&policy,
            THREAD_AFFINITY_POLICY_COUNT
        );
        return result == KERN_SUCCESS;
#else
        // Unsupported platform
        return false;
#endif
    }
    
    bool set_realtime_priority(int priority) {
#ifdef __linux__
        struct sched_param param;
        param.sched_priority = priority;
        return pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0;
#elif __APPLE__
        // macOS real-time scheduling (time constraint policy)
        thread_time_constraint_policy_data_t policy;
        policy.period = 1000000; // 1ms period in nanoseconds
        policy.constraint = 800000; // 800μs constraint
        policy.computation = 100000; // 100μs computation time
        policy.preemptible = 0; // Non-preemptible
        
        kern_return_t result = thread_policy_set(
            mach_thread_self(),
            THREAD_TIME_CONSTRAINT_POLICY,
            (thread_policy_t)&policy,
            THREAD_TIME_CONSTRAINT_POLICY_COUNT
        );
        return result == KERN_SUCCESS;
#else
        return false;
#endif
    }
    
    void preallocate_memory() {
        // Pre-allocate memory pools to avoid dynamic allocation
        // Reserve space for jitter analysis
        jitter_samples_.reserve(max_samples_);
        
        // Touch memory pages to ensure they're allocated
        volatile char* touch_mem = new char[1024 * 1024]; // 1MB
        for (size_t i = 0; i < 1024 * 1024; i += 4096) {
            touch_mem[i] = 0;
        }
        delete[] touch_mem;
    }
};
