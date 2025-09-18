#pragma once
#include <cstdint>
#include <chrono>
#include <string>

/**
 * @brief Core telemetry data sample for beamline control system
 * 
 * This structure defines the standard telemetry format exchanged between
 * the real-time control loop, data logging, and operator interfaces.
 * Designed for high-frequency sampling (1kHz) with minimal overhead.
 */
struct TelemetrySample {
    // Timing information
    double t_sec;           ///< Timestamp in seconds since control loop start
    std::uint64_t cycle;    ///< Control loop cycle counter for debugging
    
    // Primary measurements
    double pos;             ///< Beam position in mm (from BPM)
    double intensity;       ///< Beam intensity in arbitrary units (from BIC)
    
    // Control system state
    double magnet_current;  ///< Magnet current in Amperes
    double setpoint;        ///< Position setpoint in mm
    
    // Control performance metrics
    double error;           ///< Position error (setpoint - measurement) in mm
    double pid_p;           ///< Proportional term contribution
    double pid_i;           ///< Integral term contribution  
    double pid_d;           ///< Derivative term contribution
    double control_output;  ///< Total PID output before limiting
    
    // System health indicators
    bool deadline_miss;     ///< True if control loop missed timing deadline
    bool magnet_saturated;  ///< True if magnet output is at limits
    bool integrator_saturated; ///< True if PID integrator is at limits
    
    // Performance monitoring
    std::uint32_t loop_time_us; ///< Control loop execution time in microseconds
    double cpu_usage;       ///< CPU usage percentage (0.0-1.0)
    
    /**
     * @brief Default constructor - initializes all fields to safe values
     */
    TelemetrySample() 
        : t_sec(0.0)
        , cycle(0)
        , pos(0.0)
        , intensity(0.0)
        , magnet_current(0.0)
        , setpoint(0.0)
        , error(0.0)
        , pid_p(0.0)
        , pid_i(0.0)
        , pid_d(0.0)
        , control_output(0.0)
        , deadline_miss(false)
        , magnet_saturated(false)
        , integrator_saturated(false)
        , loop_time_us(0)
        , cpu_usage(0.0)
    {}
    
    /**
     * @brief Create timestamp from steady clock
     * @param start_time Reference time point for relative timestamps
     * @return Current time in seconds since start_time
     */
    static double timestamp_from_steady_clock(
        const std::chrono::steady_clock::time_point& start_time) {
        auto now = std::chrono::steady_clock::now();
        auto duration = now - start_time;
        return std::chrono::duration<double>(duration).count();
    }
    
    /**
     * @brief Check if sample indicates system is healthy
     * @return true if no error conditions are present
     */
    bool is_healthy() const {
        return !deadline_miss && !magnet_saturated && !integrator_saturated;
    }
    
    /**
     * @brief Get total PID output (sum of P, I, D terms)
     * @return Combined PID contribution
     */
    double get_pid_total() const {
        return pid_p + pid_i + pid_d;
    }
    
    /**
     * @brief Check if position is within tolerance of setpoint
     * @param tolerance Allowable deviation in mm
     * @return true if |error| <= tolerance
     */
    bool position_in_tolerance(double tolerance) const {
        return std::abs(error) <= tolerance;
    }
    
    /**
     * @brief Format sample as human-readable string for debugging
     * @return Formatted telemetry data
     */
    std::string to_string() const {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
            "TelemetrySample{t=%.3fs, cycle=%llu, pos=%.3fmm, intensity=%.1f, "
            "magnet=%.3fA, setpoint=%.3fmm, error=%.3fmm, "
            "PID=[P:%.3f,I:%.3f,D:%.3f], output=%.3f, "
            "health=[deadline:%s,mag_sat:%s,int_sat:%s], "
            "timing=%uμs, cpu=%.1f%%}",
            t_sec, cycle, pos, intensity, magnet_current, setpoint, error,
            pid_p, pid_i, pid_d, control_output,
            deadline_miss ? "MISS" : "OK",
            magnet_saturated ? "SAT" : "OK", 
            integrator_saturated ? "SAT" : "OK",
            loop_time_us, cpu_usage * 100.0);
        return std::string(buffer);
    }
};

/**
 * @brief Extended telemetry sample with additional diagnostic information
 * 
 * Used for detailed analysis and debugging. Not sent at high frequency
 * to avoid overwhelming the communication channels.
 */
struct ExtendedTelemetrySample : public TelemetrySample {
    // Hardware diagnostics
    double bpm_noise_level;     ///< Estimated noise level from BPM
    double magnet_temperature;  ///< Magnet temperature in Celsius
    double power_supply_voltage; ///< Power supply voltage
    
    // Control system diagnostics  
    double loop_jitter_us;      ///< Timing jitter in microseconds
    std::uint32_t missed_deadlines; ///< Cumulative deadline misses
    double integrator_value;    ///< Current PID integrator state
    
    // Network/communication status
    std::uint32_t telemetry_drops; ///< Number of dropped telemetry packets
    std::uint32_t command_latency_us; ///< Command processing latency
    
    /**
     * @brief Default constructor
     */
    ExtendedTelemetrySample() 
        : TelemetrySample()
        , bpm_noise_level(0.0)
        , magnet_temperature(25.0)  // Room temperature default
        , power_supply_voltage(0.0)
        , loop_jitter_us(0.0)
        , missed_deadlines(0)
        , integrator_value(0.0)
        , telemetry_drops(0)
        , command_latency_us(0)
    {}
};

/**
 * @brief Telemetry statistics for performance monitoring
 * 
 * Accumulated statistics over a time window for system health monitoring.
 */
struct TelemetryStats {
    std::uint64_t sample_count{0};
    
    // Position statistics
    double pos_mean{0.0};
    double pos_std{0.0};
    double pos_min{0.0};
    double pos_max{0.0};
    
    // Error statistics
    double error_rms{0.0};
    double error_mean{0.0};
    double max_error{0.0};
    
    // Timing statistics
    double mean_loop_time_us{0.0};
    double max_loop_time_us{0.0};
    std::uint32_t deadline_miss_count{0};
    double deadline_miss_rate{0.0};
    
    // Control performance
    double mean_cpu_usage{0.0};
    double max_cpu_usage{0.0};
    std::uint32_t saturation_events{0};
    
    /**
     * @brief Reset all statistics
     */
    void reset() {
        *this = TelemetryStats{};
    }
    
    /**
     * @brief Check if statistics indicate healthy operation
     * @return true if all metrics are within acceptable ranges
     */
    bool is_healthy() const {
        return deadline_miss_rate < 0.01 &&  // < 1% deadline misses
               max_loop_time_us < 500.0 &&   // < 500μs loop time
               max_cpu_usage < 0.8;          // < 80% CPU usage
    }
};
