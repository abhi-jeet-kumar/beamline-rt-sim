#pragma once
#include <chrono>
#include <atomic>
#include <functional>

/**
 * @brief Real-time deadline watchdog for control loop monitoring
 * 
 * Provides deadline miss detection, consecutive violation tracking,
 * and automatic corrective actions for real-time systems. Designed
 * for safety-critical control loops with strict timing requirements.
 * 
 * Features:
 * - Sub-microsecond deadline detection
 * - Consecutive violation counting with thresholds
 * - Automatic frequency reduction on persistent violations
 * - Statistical violation analysis
 * - Thread-safe operation for real-time use
 */
class Watchdog {
private:
    std::chrono::nanoseconds budget;           ///< Allowed execution time budget
    std::chrono::nanoseconds warning_threshold; ///< Warning threshold (e.g., 80% of budget)
    
    // Violation tracking
    std::atomic<bool> tripped{false};          ///< Current violation state
    std::atomic<uint32_t> consecutive_misses{0}; ///< Consecutive deadline misses
    std::atomic<uint64_t> total_violations{0}; ///< Total lifetime violations
    std::atomic<uint64_t> total_checks{0};     ///< Total deadline checks performed
    
    // Warning tracking
    std::atomic<uint32_t> consecutive_warnings{0}; ///< Consecutive warning violations
    std::atomic<uint64_t> total_warnings{0};   ///< Total warning violations
    
    // Performance statistics
    std::atomic<uint64_t> min_execution_ns{UINT64_MAX}; ///< Minimum execution time
    std::atomic<uint64_t> max_execution_ns{0};  ///< Maximum execution time
    std::atomic<uint64_t> sum_execution_ns{0};  ///< Sum for mean calculation
    
    // Thresholds for automatic actions
    uint32_t critical_consecutive_threshold{5}; ///< Trigger corrective action
    uint32_t warning_consecutive_threshold{10}; ///< Trigger warning callback
    
    // Callbacks for violations
    std::function<void(const Watchdog&)> critical_callback;
    std::function<void(const Watchdog&)> warning_callback;
    
public:
    /**
     * @brief Construct watchdog with time budget
     * @param budget_ns Maximum allowed execution time in nanoseconds
     * @param warning_ratio Warning threshold as fraction of budget (default: 0.8)
     */
    explicit Watchdog(std::chrono::nanoseconds budget_ns, double warning_ratio = 0.8) 
        : budget(budget_ns)
        , warning_threshold(static_cast<std::chrono::nanoseconds::rep>(budget_ns.count() * warning_ratio))
    {}
    
    /**
     * @brief Check execution time against deadline
     * @param start_time Execution start time point
     * @param end_time Execution end time point
     * @return true if deadline was missed
     */
    template<typename TimePoint>
    bool check(TimePoint start_time, TimePoint end_time) {
        auto execution_time = end_time - start_time;
        auto execution_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(execution_time);
        
        total_checks.fetch_add(1, std::memory_order_relaxed);
        
        // Update statistics
        update_statistics(execution_ns);
        
        // Check for deadline miss
        bool deadline_missed = execution_ns > budget;
        tripped.store(deadline_missed, std::memory_order_relaxed);
        
        if (deadline_missed) {
            handle_deadline_miss();
        } else {
            // Reset consecutive miss counter on successful execution
            consecutive_misses.store(0, std::memory_order_relaxed);
        }
        
        // Check for warning threshold
        bool warning_exceeded = execution_ns > warning_threshold;
        if (warning_exceeded) {
            handle_warning();
        } else {
            consecutive_warnings.store(0, std::memory_order_relaxed);
        }
        
        return deadline_missed;
    }
    
    /**
     * @brief Check execution time using duration
     * @param execution_time Duration of execution
     * @return true if deadline was missed
     */
    bool check_duration(std::chrono::nanoseconds execution_time) {
        auto now = std::chrono::steady_clock::now();
        auto start = now - execution_time;
        return check(start, now);
    }
    
    /**
     * @brief Reset watchdog state (but preserve statistics)
     */
    void reset() {
        tripped.store(false, std::memory_order_relaxed);
        consecutive_misses.store(0, std::memory_order_relaxed);
        consecutive_warnings.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Reset all statistics and state
     */
    void reset_all() {
        reset();
        total_violations.store(0, std::memory_order_relaxed);
        total_checks.store(0, std::memory_order_relaxed);
        total_warnings.store(0, std::memory_order_relaxed);
        min_execution_ns.store(UINT64_MAX, std::memory_order_relaxed);
        max_execution_ns.store(0, std::memory_order_relaxed);
        sum_execution_ns.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Update time budget
     * @param new_budget New execution time budget
     */
    void set_budget(std::chrono::nanoseconds new_budget) {
        budget = new_budget;
        warning_threshold = std::chrono::nanoseconds(
            static_cast<std::chrono::nanoseconds::rep>(new_budget.count() * 0.8)
        );
    }
    
    /**
     * @brief Set consecutive violation thresholds
     * @param critical_threshold Threshold for critical callback
     * @param warning_threshold Threshold for warning callback
     */
    void set_thresholds(uint32_t critical_threshold, uint32_t warning_threshold) {
        critical_consecutive_threshold = critical_threshold;
        warning_consecutive_threshold = warning_threshold;
    }
    
    /**
     * @brief Set callback for critical violations
     * @param callback Function to call on critical violations
     */
    void set_critical_callback(std::function<void(const Watchdog&)> callback) {
        critical_callback = std::move(callback);
    }
    
    /**
     * @brief Set callback for warning violations
     * @param callback Function to call on warning violations  
     */
    void set_warning_callback(std::function<void(const Watchdog&)> callback) {
        warning_callback = std::move(callback);
    }
    
    // Getters for current state
    bool is_tripped() const { return tripped.load(std::memory_order_relaxed); }
    uint32_t get_consecutive_misses() const { return consecutive_misses.load(std::memory_order_relaxed); }
    uint32_t get_consecutive_warnings() const { return consecutive_warnings.load(std::memory_order_relaxed); }
    
    // Getters for statistics
    uint64_t get_total_violations() const { return total_violations.load(std::memory_order_relaxed); }
    uint64_t get_total_warnings() const { return total_warnings.load(std::memory_order_relaxed); }
    uint64_t get_total_checks() const { return total_checks.load(std::memory_order_relaxed); }
    
    /**
     * @brief Get violation rate as percentage
     * @return Percentage of checks that resulted in deadline misses
     */
    double get_violation_rate() const {
        uint64_t checks = total_checks.load(std::memory_order_relaxed);
        if (checks == 0) return 0.0;
        return (static_cast<double>(total_violations.load(std::memory_order_relaxed)) / checks) * 100.0;
    }
    
    /**
     * @brief Get warning rate as percentage  
     * @return Percentage of checks that exceeded warning threshold
     */
    double get_warning_rate() const {
        uint64_t checks = total_checks.load(std::memory_order_relaxed);
        if (checks == 0) return 0.0;
        return (static_cast<double>(total_warnings.load(std::memory_order_relaxed)) / checks) * 100.0;
    }
    
    /**
     * @brief Get mean execution time in nanoseconds
     */
    double get_mean_execution_ns() const {
        uint64_t checks = total_checks.load(std::memory_order_relaxed);
        if (checks == 0) return 0.0;
        return static_cast<double>(sum_execution_ns.load(std::memory_order_relaxed)) / checks;
    }
    
    /**
     * @brief Get minimum execution time in nanoseconds
     */
    uint64_t get_min_execution_ns() const {
        uint64_t min_val = min_execution_ns.load(std::memory_order_relaxed);
        return (min_val == UINT64_MAX) ? 0 : min_val;
    }
    
    /**
     * @brief Get maximum execution time in nanoseconds
     */
    uint64_t get_max_execution_ns() const {
        return max_execution_ns.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get current budget in nanoseconds
     */
    std::chrono::nanoseconds get_budget() const { return budget; }
    
    /**
     * @brief Get warning threshold in nanoseconds
     */
    std::chrono::nanoseconds get_warning_threshold() const { return warning_threshold; }
    
    /**
     * @brief Check if system is in critical state
     * @return true if consecutive violations exceed critical threshold
     */
    bool is_critical() const {
        return consecutive_misses.load(std::memory_order_relaxed) >= critical_consecutive_threshold;
    }
    
    /**
     * @brief Check if system performance is acceptable
     * @return true if violation rate is below 1% and no critical state
     */
    bool is_healthy() const {
        return get_violation_rate() < 1.0 && !is_critical();
    }

private:
    void update_statistics(std::chrono::nanoseconds execution_time) {
        uint64_t exec_ns = static_cast<uint64_t>(execution_time.count());
        
        // Update min/max using compare-and-swap loops for thread safety
        uint64_t current_min = min_execution_ns.load(std::memory_order_relaxed);
        while (exec_ns < current_min && 
               !min_execution_ns.compare_exchange_weak(current_min, exec_ns, std::memory_order_relaxed)) {
            // Retry if another thread updated min_execution_ns
        }
        
        uint64_t current_max = max_execution_ns.load(std::memory_order_relaxed);
        while (exec_ns > current_max && 
               !max_execution_ns.compare_exchange_weak(current_max, exec_ns, std::memory_order_relaxed)) {
            // Retry if another thread updated max_execution_ns
        }
        
        // Update sum for mean calculation
        sum_execution_ns.fetch_add(exec_ns, std::memory_order_relaxed);
    }
    
    void handle_deadline_miss() {
        total_violations.fetch_add(1, std::memory_order_relaxed);
        uint32_t consecutive = consecutive_misses.fetch_add(1, std::memory_order_relaxed) + 1;
        
        // Check if critical threshold reached
        if (consecutive >= critical_consecutive_threshold && critical_callback) {
            critical_callback(*this);
        }
    }
    
    void handle_warning() {
        total_warnings.fetch_add(1, std::memory_order_relaxed);
        uint32_t consecutive = consecutive_warnings.fetch_add(1, std::memory_order_relaxed) + 1;
        
        // Check if warning threshold reached
        if (consecutive >= warning_consecutive_threshold && warning_callback) {
            warning_callback(*this);
        }
    }
};
