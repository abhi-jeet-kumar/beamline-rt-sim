#pragma once
#include "isensor.hpp"
#include "sim_noise.hpp"
#include <chrono>
#include <atomic>
#include <cmath>

/**
 * @brief Beam Position Monitor (BPM) simulation
 * 
 * Simulates a 4-electrode BPM with realistic physics modeling:
 * - Position calculation from electrode signals
 * - Beam current dependent shot noise
 * - Electronics and thermal noise
 * - Mechanical vibrations and drift
 * - Calibration offsets and scaling
 * - Temperature and environmental effects
 * 
 * Designed for high-frequency readout (1kHz+) with minimal computational overhead.
 */
class BPM : public ISensor {
private:
    // Physical parameters
    std::atomic<double> beam_position_x_{0.0};      ///< True beam position X (mm)
    std::atomic<double> beam_position_y_{0.0};      ///< True beam position Y (mm) 
    std::atomic<double> beam_current_{100.0};       ///< Beam current (mA)
    std::atomic<double> chamber_radius_{10.0};      ///< BPM chamber radius (mm)
    
    // Calibration parameters
    std::atomic<double> sensitivity_x_{1.0};        ///< X sensitivity (mm/mm)
    std::atomic<double> sensitivity_y_{1.0};        ///< Y sensitivity (mm/mm)
    std::atomic<double> offset_x_{0.0};            ///< X offset (mm)
    std::atomic<double> offset_y_{0.0};            ///< Y offset (mm)
    std::atomic<double> rotation_angle_{0.0};       ///< Rotation angle (radians)
    
    // Noise simulation
    mutable BeamlineNoise::BPMNoise noise_generator_;
    std::atomic<bool> enable_noise_{true};
    
    // Performance optimization
    mutable std::atomic<uint64_t> read_count_{0};
    mutable std::atomic<double> last_reading_{0.0};
    mutable std::chrono::steady_clock::time_point last_read_time_;
    
    // Axis selection for readout
    enum class Axis { X, Y };
    std::atomic<Axis> readout_axis_{Axis::X};
    
    // Advanced physics simulation
    std::atomic<double> electrode_gain_mismatch_{0.02}; ///< Electrode gain variation (2%)
    std::atomic<double> temperature_{20.0};           ///< Operating temperature (°C)
    std::atomic<double> temperature_coefficient_{0.001}; ///< Temp coeff (mm/°C)
    
public:
    /**
     * @brief Construct BPM with specified ID
     * @param bpm_id Unique identifier for this BPM
     * @param noise_seed Random seed for noise generation (0 = random)
     */
    explicit BPM(const std::string& bpm_id = "BPM_01", uint64_t noise_seed = 0)
        : noise_generator_(noise_seed)
        , last_read_time_(std::chrono::steady_clock::now())
    {
        set_id(bpm_id);
    }
    
    /**
     * @brief Read current beam position (implements ISensor::read)
     * @return Beam position in mm (X or Y based on readout_axis)
     */
    double read() override {
        if (!initialized_) {
            throw std::runtime_error("BPM not initialized");
        }
        
        read_count_.fetch_add(1, std::memory_order_relaxed);
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_read_time_).count();
        last_read_time_ = now;
        
        // Get true beam position
        double true_x = beam_position_x_.load(std::memory_order_relaxed);
        double true_y = beam_position_y_.load(std::memory_order_relaxed);
        
        // Apply rotation transformation
        double angle = rotation_angle_.load(std::memory_order_relaxed);
        double rotated_x = true_x * std::cos(angle) - true_y * std::sin(angle);
        double rotated_y = true_x * std::sin(angle) + true_y * std::cos(angle);
        
        // Select axis for readout
        double true_position = (readout_axis_.load() == Axis::X) ? rotated_x : rotated_y;
        double sensitivity = (readout_axis_.load() == Axis::X) ? 
                           sensitivity_x_.load() : sensitivity_y_.load();
        double offset = (readout_axis_.load() == Axis::X) ? 
                       offset_x_.load() : offset_y_.load();
        
        // Apply calibration
        double calibrated_position = true_position * sensitivity + offset;
        
        // Add temperature effects
        double temp = temperature_.load(std::memory_order_relaxed);
        double temp_coeff = temperature_coefficient_.load(std::memory_order_relaxed);
        calibrated_position += (temp - 20.0) * temp_coeff;
        
        // Add noise if enabled
        double measured_position = calibrated_position;
        if (enable_noise_.load(std::memory_order_relaxed)) {
            double current = beam_current_.load(std::memory_order_relaxed);
            double noise = noise_generator_.generate(current, dt);
            measured_position += noise;
        }
        
        // Add electrode gain mismatch (systematic error)
        double gain_error = electrode_gain_mismatch_.load(std::memory_order_relaxed);
        if (gain_error > 0.0) {
            // Simulate electrode imbalance effect
            double radius = chamber_radius_.load(std::memory_order_relaxed);
            double normalized_pos = measured_position / radius;
            double nonlinearity = gain_error * normalized_pos * normalized_pos;
            measured_position += nonlinearity;
        }
        
        last_reading_.store(measured_position, std::memory_order_relaxed);
        return measured_position;
    }
    
    /**
     * @brief Initialize BPM hardware simulation
     */
    bool initialize() override {
        if (!ISensor::initialize()) {
            return false;
        }
        
        // Reset statistics
        read_count_.store(0, std::memory_order_relaxed);
        last_read_time_ = std::chrono::steady_clock::now();
        
        // Initialize noise generator settings
        noise_generator_.set_noise_levels(0.001, 0.0005, 0.01); // thermal, electronics, vibration
        
        return true;
    }
    
    /**
     * @brief Set true beam position (for simulation control)
     * @param x_mm X position in mm
     * @param y_mm Y position in mm
     */
    void set_beam_position(double x_mm, double y_mm) {
        beam_position_x_.store(x_mm, std::memory_order_relaxed);
        beam_position_y_.store(y_mm, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get true beam position
     * @return pair of (x_mm, y_mm)
     */
    std::pair<double, double> get_beam_position() const {
        return {beam_position_x_.load(std::memory_order_relaxed),
                beam_position_y_.load(std::memory_order_relaxed)};
    }
    
    /**
     * @brief Set beam current (affects shot noise)
     * @param current_ma Beam current in mA
     */
    void set_beam_current(double current_ma) {
        beam_current_.store(current_ma, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get beam current
     */
    double get_beam_current() const {
        return beam_current_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Set readout axis
     * @param axis_name "X" or "Y"
     */
    void set_readout_axis(const std::string& axis_name) {
        if (axis_name == "X" || axis_name == "x") {
            readout_axis_.store(Axis::X, std::memory_order_relaxed);
        } else if (axis_name == "Y" || axis_name == "y") {
            readout_axis_.store(Axis::Y, std::memory_order_relaxed);
        }
    }
    
    /**
     * @brief Get current readout axis
     */
    std::string get_readout_axis() const {
        return (readout_axis_.load() == Axis::X) ? "X" : "Y";
    }
    
    /**
     * @brief Set BPM calibration parameters
     * @param sens_x X sensitivity (mm/mm)
     * @param sens_y Y sensitivity (mm/mm)
     * @param off_x X offset (mm)
     * @param off_y Y offset (mm)
     * @param rotation Rotation angle (degrees)
     */
    void set_calibration(double sens_x, double sens_y, double off_x, double off_y, double rotation = 0.0) {
        sensitivity_x_.store(sens_x, std::memory_order_relaxed);
        sensitivity_y_.store(sens_y, std::memory_order_relaxed);
        offset_x_.store(off_x, std::memory_order_relaxed);
        offset_y_.store(off_y, std::memory_order_relaxed);
        rotation_angle_.store(rotation * M_PI / 180.0, std::memory_order_relaxed); // Convert to radians
    }
    
    /**
     * @brief Get calibration parameters
     * @return tuple of (sens_x, sens_y, off_x, off_y, rotation_degrees)
     */
    std::tuple<double, double, double, double, double> get_calibration() const {
        double rot_rad = rotation_angle_.load(std::memory_order_relaxed);
        return {sensitivity_x_.load(), sensitivity_y_.load(), 
                offset_x_.load(), offset_y_.load(), 
                rot_rad * 180.0 / M_PI}; // Convert to degrees
    }
    
    /**
     * @brief Set operating temperature
     * @param temp_celsius Temperature in Celsius
     */
    void set_temperature(double temp_celsius) {
        temperature_.store(temp_celsius, std::memory_order_relaxed);
    }
    
    /**
     * @brief Enable or disable noise simulation
     * @param enable true to enable noise
     */
    void enable_noise(bool enable) {
        enable_noise_.store(enable, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set chamber radius for nonlinearity calculation
     * @param radius_mm Chamber radius in mm
     */
    void set_chamber_radius(double radius_mm) {
        chamber_radius_.store(radius_mm, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set electrode gain mismatch level
     * @param mismatch Fractional gain variation (0.02 = 2%)
     */
    void set_electrode_gain_mismatch(double mismatch) {
        electrode_gain_mismatch_.store(mismatch, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total number of reads performed
     */
    uint64_t get_read_count() const {
        return read_count_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get last reading without performing new measurement
     */
    double get_last_reading() const {
        return last_reading_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Perform self-test by checking calibration validity
     */
    bool self_test() override {
        if (!initialized_) return false;
        
        // Check calibration parameters are reasonable
        double sens_x = sensitivity_x_.load();
        double sens_y = sensitivity_y_.load();
        
        if (sens_x <= 0.1 || sens_x >= 10.0) return false;  // Sensitivity out of range
        if (sens_y <= 0.1 || sens_y >= 10.0) return false;
        
        // Test readout
        try {
            double old_pos_x = beam_position_x_.load();
            double old_pos_y = beam_position_y_.load();
            
            // Set known position and verify readout
            set_beam_position(1.0, 0.0);
            set_readout_axis("X");
            enable_noise(false);
            
            double reading = read();
            
            // Restore original state
            set_beam_position(old_pos_x, old_pos_y);
            enable_noise(true);
            
            // Check if reading is reasonable
            return std::abs(reading - 1.0) < 0.5; // Allow for calibration offset
            
        } catch (...) {
            return false;
        }
    }
    
    // ISensor interface implementations
    std::string get_type_name() const override { return "BPM"; }
    std::string get_units() const override { return "mm"; }
    std::pair<double, double> get_range() const override { 
        double radius = chamber_radius_.load();
        return {-radius, radius}; 
    }
    double get_resolution() const override { return 0.001; } // 1 μm resolution
};
