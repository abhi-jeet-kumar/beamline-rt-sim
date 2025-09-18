#pragma once
#include <chrono>
#include <string>
#include <atomic>
#include <cmath>

/**
 * @brief Abstract actuator interface for FESA-style hardware abstraction
 * 
 * Provides a standardized interface for all actuator types in the beamline
 * control system. Designed for high-frequency control updates (1kHz+) with
 * safety limits, diagnostics, and comprehensive state monitoring.
 * 
 * Features:
 * - Pure virtual interface for polymorphic actuator access
 * - Safety limits and range checking
 * - Error state management and diagnostics
 * - Performance monitoring and statistics
 * - Thread-safe operation support
 * - Ramping and rate limiting capabilities
 */
class IActuator {
public:
    /**
     * @brief Actuator error states
     */
    enum class ErrorState {
        OK = 0,                    ///< Normal operation
        OUT_OF_RANGE,             ///< Commanded value outside safe range
        RATE_LIMIT_EXCEEDED,      ///< Rate of change too high
        COMMUNICATION_ERROR,       ///< Communication with hardware failed
        HARDWARE_FAULT,           ///< Hardware malfunction detected
        SAFETY_INTERLOCK,         ///< Safety system preventing operation
        POWER_FAULT,              ///< Power supply or amplifier fault
        OVERTEMPERATURE,          ///< Thermal protection triggered
        NOT_INITIALIZED,          ///< Actuator not properly initialized
        UNKNOWN_ERROR             ///< Unspecified error condition
    };

    /**
     * @brief Actuator operation result
     */
    struct SetResult {
        bool success{false};                           ///< Operation success flag
        double actual_value{0.0};                     ///< Actual value achieved
        double commanded_value{0.0};                  ///< Value that was commanded
        ErrorState error{ErrorState::OK};             ///< Error state if any
        std::chrono::steady_clock::time_point timestamp; ///< When operation completed
        double execution_time_us{0.0};                ///< Time taken for operation
        
        SetResult() : timestamp(std::chrono::steady_clock::now()) {}
        
        SetResult(bool succ, double actual, double commanded, ErrorState err = ErrorState::OK)
            : success(succ), actual_value(actual), commanded_value(commanded)
            , error(err), timestamp(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Actuator performance statistics
     */
    struct Statistics {
        uint64_t total_commands{0};              ///< Total number of commands sent
        uint64_t successful_commands{0};         ///< Number of successful commands
        uint64_t error_count{0};                 ///< Total number of errors
        uint64_t range_violations{0};            ///< Commands outside safe range
        uint64_t rate_violations{0};             ///< Rate limit violations
        double mean_command_time_us{0.0};        ///< Mean command execution time
        double max_command_time_us{0.0};         ///< Maximum command time observed
        double success_rate{100.0};              ///< Success rate percentage
        double min_commanded{0.0};               ///< Minimum value commanded
        double max_commanded{0.0};               ///< Maximum value commanded
        std::chrono::steady_clock::time_point last_command_time; ///< Last command timestamp
        
        void update_on_success(double commanded_value, double execution_time_us) {
            total_commands++;
            successful_commands++;
            last_command_time = std::chrono::steady_clock::now();
            
            // Update timing statistics
            mean_command_time_us = ((mean_command_time_us * (successful_commands - 1)) + execution_time_us) / successful_commands;
            if (execution_time_us > max_command_time_us) {
                max_command_time_us = execution_time_us;
            }
            
            // Update range statistics
            if (total_commands == 1) {
                min_commanded = max_commanded = commanded_value;
            } else {
                if (commanded_value < min_commanded) min_commanded = commanded_value;
                if (commanded_value > max_commanded) max_commanded = commanded_value;
            }
            
            success_rate = (static_cast<double>(successful_commands) / total_commands) * 100.0;
        }
        
        void update_on_error(ErrorState error_type) {
            total_commands++;
            error_count++;
            if (error_type == ErrorState::OUT_OF_RANGE) {
                range_violations++;
            } else if (error_type == ErrorState::RATE_LIMIT_EXCEEDED) {
                rate_violations++;
            }
            last_command_time = std::chrono::steady_clock::now();
            success_rate = (static_cast<double>(successful_commands) / total_commands) * 100.0;
        }
    };

protected:
    mutable Statistics stats_;                  ///< Performance statistics
    ErrorState last_error_{ErrorState::OK};    ///< Last error encountered
    std::string actuator_id_;                   ///< Unique actuator identifier
    bool initialized_{false};                  ///< Initialization state
    std::atomic<double> current_value_{0.0};   ///< Current actuator value
    std::atomic<double> target_value_{0.0};    ///< Target/commanded value

    // Safety limits
    double min_value_{-1e6};                    ///< Minimum safe value
    double max_value_{1e6};                     ///< Maximum safe value
    double max_rate_{1e6};                      ///< Maximum rate of change per second
    
    // Last command tracking for rate limiting
    mutable std::chrono::steady_clock::time_point last_command_time_;
    mutable double last_commanded_value_{0.0};

public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~IActuator() = default;

    /**
     * @brief Set actuator to specified value (pure virtual)
     * @param value Target value to set
     */
    virtual void set(double value) = 0;

    /**
     * @brief Get current actuator value (pure virtual)
     * @return Current actuator value
     */
    virtual double get() const = 0;

    /**
     * @brief Set actuator value with comprehensive result reporting
     * @param value Target value to set
     * @return SetResult with success status and metadata
     */
    virtual SetResult set_with_result(double value) {
        auto start_time = std::chrono::steady_clock::now();
        
        // Check if initialized
        if (!initialized_) {
            last_error_ = ErrorState::NOT_INITIALIZED;
            stats_.update_on_error(last_error_);
            return SetResult(false, get(), value, last_error_);
        }
        
        // Validate range
        if (value < min_value_ || value > max_value_) {
            last_error_ = ErrorState::OUT_OF_RANGE;
            stats_.update_on_error(last_error_);
            return SetResult(false, get(), value, last_error_);
        }
        
        // Check rate limit
        auto now = std::chrono::steady_clock::now();
        if (last_command_time_.time_since_epoch().count() > 0) {
            double dt = std::chrono::duration<double>(now - last_command_time_).count();
            if (dt > 0) {
                double rate = std::abs(value - last_commanded_value_) / dt;
                if (rate > max_rate_) {
                    last_error_ = ErrorState::RATE_LIMIT_EXCEEDED;
                    stats_.update_on_error(last_error_);
                    return SetResult(false, get(), value, last_error_);
                }
            }
        }
        
        try {
            set(value);
            target_value_.store(value, std::memory_order_relaxed);
            
            auto end_time = std::chrono::steady_clock::now();
            double execution_time_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
            
            last_error_ = ErrorState::OK;
            last_command_time_ = now;
            last_commanded_value_ = value;
            stats_.update_on_success(value, execution_time_us);
            
            double actual = get();
            current_value_.store(actual, std::memory_order_relaxed);
            
            return SetResult(true, actual, value, ErrorState::OK);
            
        } catch (...) {
            last_error_ = ErrorState::UNKNOWN_ERROR;
            stats_.update_on_error(last_error_);
            return SetResult(false, get(), value, last_error_);
        }
    }

    /**
     * @brief Initialize actuator hardware
     * @return true if initialization successful
     */
    virtual bool initialize() {
        initialized_ = true;
        current_value_.store(get(), std::memory_order_relaxed);
        target_value_.store(get(), std::memory_order_relaxed);
        return true;
    }

    /**
     * @brief Shutdown actuator and move to safe state
     */
    virtual void shutdown() {
        // Move to safe state (typically zero or park position)
        try {
            set(0.0);
        } catch (...) {
            // Best effort - continue with shutdown
        }
        initialized_ = false;
    }

    /**
     * @brief Check if actuator is properly initialized
     */
    virtual bool is_initialized() const {
        return initialized_;
    }

    /**
     * @brief Set safety limits
     * @param min_val Minimum safe value
     * @param max_val Maximum safe value
     */
    virtual void set_limits(double min_val, double max_val) {
        min_value_ = min_val;
        max_value_ = max_val;
    }

    /**
     * @brief Set maximum rate of change
     * @param max_rate_per_sec Maximum rate in units per second
     */
    virtual void set_rate_limit(double max_rate_per_sec) {
        max_rate_ = max_rate_per_sec;
    }

    /**
     * @brief Get current safety limits
     * @return pair of (min_value, max_value)
     */
    virtual std::pair<double, double> get_limits() const {
        return {min_value_, max_value_};
    }

    /**
     * @brief Get maximum rate limit
     */
    virtual double get_rate_limit() const {
        return max_rate_;
    }

    /**
     * @brief Get actuator identifier
     */
    virtual std::string get_id() const {
        return actuator_id_;
    }

    /**
     * @brief Set actuator identifier
     */
    virtual void set_id(const std::string& id) {
        actuator_id_ = id;
    }

    /**
     * @brief Get last error state
     */
    ErrorState get_last_error() const {
        return last_error_;
    }

    /**
     * @brief Get performance statistics
     */
    const Statistics& get_statistics() const {
        return stats_;
    }

    /**
     * @brief Reset performance statistics
     */
    virtual void reset_statistics() {
        stats_ = Statistics{};
    }

    /**
     * @brief Get actuator type name (for debugging/logging)
     */
    virtual std::string get_type_name() const = 0;

    /**
     * @brief Get actuator units (e.g., "A", "mm", "degrees")
     */
    virtual std::string get_units() const = 0;

    /**
     * @brief Get actuator resolution (smallest controllable change)
     */
    virtual double get_resolution() const = 0;

    /**
     * @brief Check if actuator is currently healthy
     * @return true if actuator is operational
     */
    virtual bool is_healthy() const {
        return initialized_ && 
               last_error_ == ErrorState::OK && 
               stats_.success_rate > 95.0; // >95% success rate for actuators
    }

    /**
     * @brief Check if actuator is at target position
     * @param tolerance Acceptable deviation from target
     * @return true if within tolerance of target
     */
    virtual bool is_at_target(double tolerance = 0.01) const {
        double current = current_value_.load(std::memory_order_relaxed);
        double target = target_value_.load(std::memory_order_relaxed);
        return std::abs(current - target) <= tolerance;
    }

    /**
     * @brief Get target value
     */
    virtual double get_target() const {
        return target_value_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Perform actuator self-test
     * @return true if self-test passes
     */
    virtual bool self_test() = 0;

    /**
     * @brief Emergency stop - immediately halt actuator
     */
    virtual void emergency_stop() {
        // Default implementation just holds current position
        try {
            double current = get();
            set(current);
            target_value_.store(current, std::memory_order_relaxed);
        } catch (...) {
            // Best effort during emergency
        }
    }

    /**
     * @brief Convert error state to human-readable string
     */
    static std::string error_to_string(ErrorState error) {
        switch (error) {
            case ErrorState::OK: return "OK";
            case ErrorState::OUT_OF_RANGE: return "OUT_OF_RANGE";
            case ErrorState::RATE_LIMIT_EXCEEDED: return "RATE_LIMIT_EXCEEDED";
            case ErrorState::COMMUNICATION_ERROR: return "COMMUNICATION_ERROR";
            case ErrorState::HARDWARE_FAULT: return "HARDWARE_FAULT";
            case ErrorState::SAFETY_INTERLOCK: return "SAFETY_INTERLOCK";
            case ErrorState::POWER_FAULT: return "POWER_FAULT";
            case ErrorState::OVERTEMPERATURE: return "OVERTEMPERATURE";
            case ErrorState::NOT_INITIALIZED: return "NOT_INITIALIZED";
            case ErrorState::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
            default: return "INVALID_ERROR_STATE";
        }
    }
};
