#pragma once
#include <chrono>
#include <string>
#include <memory>

/**
 * @brief Abstract sensor interface for FESA-style hardware abstraction
 * 
 * Provides a standardized interface for all sensor types in the beamline
 * control system. Designed for high-frequency polling (1kHz+) with
 * minimal overhead and comprehensive diagnostics.
 * 
 * Features:
 * - Pure virtual interface for polymorphic sensor access
 * - Error state management and diagnostics
 * - Performance monitoring capabilities
 * - Timeout and validity checking
 * - Thread-safe operation support
 */
class ISensor {
public:
    /**
     * @brief Sensor error states
     */
    enum class ErrorState {
        OK = 0,                    ///< Normal operation
        TIMEOUT,                   ///< Read operation timed out
        COMMUNICATION_ERROR,       ///< Communication with hardware failed
        OUT_OF_RANGE,             ///< Reading is outside valid range
        CALIBRATION_ERROR,        ///< Sensor calibration is invalid
        HARDWARE_FAULT,           ///< Hardware malfunction detected
        NOT_INITIALIZED,          ///< Sensor not properly initialized
        UNKNOWN_ERROR             ///< Unspecified error condition
    };

    /**
     * @brief Sensor reading with metadata
     */
    struct Reading {
        double value{0.0};                              ///< Sensor reading value
        std::chrono::steady_clock::time_point timestamp; ///< When reading was taken
        ErrorState error{ErrorState::OK};              ///< Error state
        bool valid{false};                             ///< Reading validity flag
        double quality{1.0};                          ///< Reading quality (0.0-1.0)
        
        Reading() : timestamp(std::chrono::steady_clock::now()) {}
        
        Reading(double val, ErrorState err = ErrorState::OK, double qual = 1.0)
            : value(val), timestamp(std::chrono::steady_clock::now())
            , error(err), valid(err == ErrorState::OK), quality(qual) {}
        
        /**
         * @brief Check if reading is valid and recent
         * @param max_age Maximum age for reading to be considered fresh
         * @return true if reading is valid and within max_age
         */
        bool is_fresh(std::chrono::milliseconds max_age = std::chrono::milliseconds(100)) const {
            if (!valid || error != ErrorState::OK) return false;
            auto age = std::chrono::steady_clock::now() - timestamp;
            return age <= max_age;
        }
        
        /**
         * @brief Get age of reading in milliseconds
         */
        double age_ms() const {
            auto age = std::chrono::steady_clock::now() - timestamp;
            return std::chrono::duration<double, std::milli>(age).count();
        }
    };

    /**
     * @brief Sensor performance statistics
     */
    struct Statistics {
        uint64_t total_reads{0};                ///< Total number of reads performed
        uint64_t successful_reads{0};           ///< Number of successful reads
        uint64_t error_count{0};                ///< Total number of errors
        uint64_t timeout_count{0};              ///< Number of timeout errors
        double mean_read_time_us{0.0};          ///< Mean read time in microseconds
        double max_read_time_us{0.0};           ///< Maximum read time observed
        double success_rate{100.0};             ///< Success rate percentage
        std::chrono::steady_clock::time_point last_read_time; ///< Timestamp of last read
        
        void update_on_success(double read_time_us) {
            total_reads++;
            successful_reads++;
            last_read_time = std::chrono::steady_clock::now();
            
            // Update timing statistics
            mean_read_time_us = ((mean_read_time_us * (successful_reads - 1)) + read_time_us) / successful_reads;
            if (read_time_us > max_read_time_us) {
                max_read_time_us = read_time_us;
            }
            
            success_rate = (static_cast<double>(successful_reads) / total_reads) * 100.0;
        }
        
        void update_on_error(ErrorState error_type) {
            total_reads++;
            error_count++;
            if (error_type == ErrorState::TIMEOUT) {
                timeout_count++;
            }
            last_read_time = std::chrono::steady_clock::now();
            success_rate = (static_cast<double>(successful_reads) / total_reads) * 100.0;
        }
    };

protected:
    mutable Statistics stats_;                  ///< Performance statistics
    ErrorState last_error_{ErrorState::OK};    ///< Last error encountered
    std::string sensor_id_;                     ///< Unique sensor identifier
    bool initialized_{false};                  ///< Initialization state

public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~ISensor() = default;

    /**
     * @brief Read current sensor value (pure virtual)
     * @return Current sensor reading
     */
    virtual double read() = 0;

    /**
     * @brief Read sensor value with full metadata
     * @return Reading structure with value, timestamp, and error state
     */
    virtual Reading read_with_metadata() {
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            double value = read();
            auto end_time = std::chrono::steady_clock::now();
            
            double read_time_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
            stats_.update_on_success(read_time_us);
            last_error_ = ErrorState::OK;
            
            return Reading(value, ErrorState::OK, 1.0);
            
        } catch (...) {
            last_error_ = ErrorState::UNKNOWN_ERROR;
            stats_.update_on_error(last_error_);
            return Reading(0.0, last_error_, 0.0);
        }
    }

    /**
     * @brief Initialize sensor hardware
     * @return true if initialization successful
     */
    virtual bool initialize() {
        initialized_ = true;
        return true;
    }

    /**
     * @brief Shutdown sensor and cleanup resources
     */
    virtual void shutdown() {
        initialized_ = false;
    }

    /**
     * @brief Check if sensor is properly initialized
     */
    virtual bool is_initialized() const {
        return initialized_;
    }

    /**
     * @brief Get sensor identifier
     */
    virtual std::string get_id() const {
        return sensor_id_;
    }

    /**
     * @brief Set sensor identifier
     */
    virtual void set_id(const std::string& id) {
        sensor_id_ = id;
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
     * @brief Get sensor type name (for debugging/logging)
     */
    virtual std::string get_type_name() const = 0;

    /**
     * @brief Get sensor units (e.g., "mm", "µA", "counts")
     */
    virtual std::string get_units() const = 0;

    /**
     * @brief Get valid measurement range
     * @return pair of (min_value, max_value)
     */
    virtual std::pair<double, double> get_range() const = 0;

    /**
     * @brief Get sensor resolution (smallest measurable change)
     */
    virtual double get_resolution() const = 0;

    /**
     * @brief Check if sensor is currently healthy
     * @return true if sensor is operational
     */
    virtual bool is_healthy() const {
        return initialized_ && 
               last_error_ == ErrorState::OK && 
               stats_.success_rate > 90.0; // >90% success rate
    }

    /**
     * @brief Perform sensor self-test
     * @return true if self-test passes
     */
    virtual bool self_test() = 0;

    /**
     * @brief Convert error state to human-readable string
     */
    static std::string error_to_string(ErrorState error) {
        switch (error) {
            case ErrorState::OK: return "OK";
            case ErrorState::TIMEOUT: return "TIMEOUT";
            case ErrorState::COMMUNICATION_ERROR: return "COMMUNICATION_ERROR";
            case ErrorState::OUT_OF_RANGE: return "OUT_OF_RANGE";
            case ErrorState::CALIBRATION_ERROR: return "CALIBRATION_ERROR";
            case ErrorState::HARDWARE_FAULT: return "HARDWARE_FAULT";
            case ErrorState::NOT_INITIALIZED: return "NOT_INITIALIZED";
            case ErrorState::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
            default: return "INVALID_ERROR_STATE";
        }
    }
};
