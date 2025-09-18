#pragma once
#include "isensor.hpp"
#include "sim_noise.hpp"
#include <chrono>
#include <atomic>
#include <cmath>

/**
 * @brief Beam Intensity Counter (BIC) simulation
 * 
 * Simulates a beam current/intensity monitor with realistic physics:
 * - Poisson counting statistics for particle detection
 * - Dark current background
 * - Gain variations and stability
 * - Electronics noise and drift
 * - Saturation and linearity effects
 * - Temperature dependence
 * 
 * Optimized for high-frequency readout with proper statistical modeling.
 */
class BIC : public ISensor {
private:
    // Physical parameters
    std::atomic<double> true_intensity_{1000.0};     ///< True beam intensity (counts/s or μA)
    std::atomic<double> dark_current_{10.0};         ///< Dark current (counts/s)
    std::atomic<double> conversion_factor_{1.0};     ///< Counts to physical units
    std::atomic<double> integration_time_{0.001};    ///< Integration time (s)
    
    // Detector characteristics
    std::atomic<double> quantum_efficiency_{0.8};    ///< Detector quantum efficiency
    std::atomic<double> gain_{1e6};                  ///< Internal gain (for PMT/avalanche detectors)
    std::atomic<double> gain_stability_{0.02};       ///< Gain variation (2% typical)
    std::atomic<double> saturation_level_{1e8};      ///< Saturation level (counts/s)
    
    // Noise simulation
    mutable BeamlineNoise::BICNoise noise_generator_;
    std::atomic<bool> enable_noise_{true};
    
    // Environmental effects
    std::atomic<double> temperature_{20.0};          ///< Operating temperature (°C)
    std::atomic<double> temp_coefficient_{0.001};    ///< Temperature coefficient (/°C)
    
    // Calibration
    std::atomic<double> calibration_offset_{0.0};    ///< Calibration offset
    std::atomic<double> calibration_scale_{1.0};     ///< Calibration scale factor
    
    // Performance tracking
    mutable std::atomic<uint64_t> read_count_{0};
    mutable std::atomic<double> last_reading_{0.0};
    mutable std::chrono::steady_clock::time_point last_read_time_;
    
    // Advanced features
    std::atomic<bool> enable_saturation_{true};      ///< Enable saturation simulation
    std::atomic<double> linearity_error_{0.001};     ///< Nonlinearity coefficient
    
public:
    /**
     * @brief Construct BIC with specified ID
     * @param bic_id Unique identifier for this BIC
     * @param noise_seed Random seed for noise generation (0 = random)
     */
    explicit BIC(const std::string& bic_id = "BIC_01", uint64_t noise_seed = 0)
        : noise_generator_(noise_seed)
        , last_read_time_(std::chrono::steady_clock::now())
    {
        set_id(bic_id);
    }
    
    /**
     * @brief Read current beam intensity (implements ISensor::read)
     * @return Measured intensity in calibrated units
     */
    double read() override {
        if (!initialized_) {
            throw std::runtime_error("BIC not initialized");
        }
        
        read_count_.fetch_add(1, std::memory_order_relaxed);
        auto now = std::chrono::steady_clock::now();
        last_read_time_ = now;
        
        // Get true intensity
        double true_intensity = true_intensity_.load(std::memory_order_relaxed);
        double dark = dark_current_.load(std::memory_order_relaxed);
        double integration_time = integration_time_.load(std::memory_order_relaxed);
        double qe = quantum_efficiency_.load(std::memory_order_relaxed);
        
        // Calculate expected counts for this integration period
        double expected_signal_counts = true_intensity * qe * integration_time;
        double expected_dark_counts = dark * integration_time;
        double total_expected = expected_signal_counts + expected_dark_counts;
        
        // Apply saturation if enabled
        if (enable_saturation_.load(std::memory_order_relaxed)) {
            double sat_level = saturation_level_.load(std::memory_order_relaxed);
            if (total_expected > sat_level * integration_time) {
                total_expected = sat_level * integration_time;
            }
        }
        
        // Generate measured intensity with noise
        double measured_intensity = total_expected / integration_time; // Convert back to rate
        
        if (enable_noise_.load(std::memory_order_relaxed)) {
            measured_intensity = noise_generator_.generate(measured_intensity);
        }
        
        // Subtract dark current from final measurement
        measured_intensity = std::max(0.0, measured_intensity - dark);
        
        // Apply gain variations
        double gain_factor = gain_.load(std::memory_order_relaxed);
        double gain_var = gain_stability_.load(std::memory_order_relaxed);
        if (enable_noise_.load() && gain_var > 0.0) {
            // Simple gain variation model
            static thread_local double gain_variation = 1.0;
            gain_variation += (noise_generator_.generate(0.1) - 0.05) * gain_var * 0.01;
            gain_variation = std::max(0.5, std::min(2.0, gain_variation)); // Clamp to reasonable range
            measured_intensity *= gain_variation;
        }
        
        // Apply temperature effects
        double temp = temperature_.load(std::memory_order_relaxed);
        double temp_coeff = temp_coefficient_.load(std::memory_order_relaxed);
        double temp_factor = 1.0 + (temp - 20.0) * temp_coeff;
        measured_intensity *= temp_factor;
        
        // Apply nonlinearity
        double linearity_err = linearity_error_.load(std::memory_order_relaxed);
        if (linearity_err > 0.0) {
            double normalized_intensity = measured_intensity / saturation_level_.load();
            double nonlinearity = linearity_err * normalized_intensity * normalized_intensity;
            measured_intensity *= (1.0 + nonlinearity);
        }
        
        // Apply calibration
        double cal_scale = calibration_scale_.load(std::memory_order_relaxed);
        double cal_offset = calibration_offset_.load(std::memory_order_relaxed);
        double calibrated_intensity = measured_intensity * cal_scale + cal_offset;
        
        // Apply conversion factor (e.g., counts/s to μA)
        double conversion = conversion_factor_.load(std::memory_order_relaxed);
        double final_reading = calibrated_intensity * conversion;
        
        last_reading_.store(final_reading, std::memory_order_relaxed);
        return final_reading;
    }
    
    /**
     * @brief Initialize BIC hardware simulation
     */
    bool initialize() override {
        if (!ISensor::initialize()) {
            return false;
        }
        
        // Reset statistics
        read_count_.store(0, std::memory_order_relaxed);
        last_read_time_ = std::chrono::steady_clock::now();
        
        // Configure noise generator
        noise_generator_.set_dark_current(dark_current_.load());
        noise_generator_.set_gain_variation(gain_stability_.load());
        
        return true;
    }
    
    /**
     * @brief Set true beam intensity (for simulation control)
     * @param intensity Beam intensity in detector units
     */
    void set_beam_intensity(double intensity) {
        true_intensity_.store(std::max(0.0, intensity), std::memory_order_relaxed);
    }
    
    /**
     * @brief Get true beam intensity
     */
    double get_beam_intensity() const {
        return true_intensity_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Set dark current level
     * @param dark_counts Dark current in counts/s
     */
    void set_dark_current(double dark_counts) {
        dark_current_.store(std::max(0.0, dark_counts), std::memory_order_relaxed);
        noise_generator_.set_dark_current(dark_counts);
    }
    
    /**
     * @brief Set detector integration time
     * @param time_s Integration time in seconds
     */
    void set_integration_time(double time_s) {
        integration_time_.store(std::max(1e-6, time_s), std::memory_order_relaxed);
    }
    
    /**
     * @brief Set quantum efficiency
     * @param efficiency Quantum efficiency (0.0 to 1.0)
     */
    void set_quantum_efficiency(double efficiency) {
        quantum_efficiency_.store(std::max(0.0, std::min(1.0, efficiency)), std::memory_order_relaxed);
    }
    
    /**
     * @brief Set detector gain and stability
     * @param gain Detector gain factor
     * @param stability Gain stability (fractional variation)
     */
    void set_gain(double gain, double stability = 0.02) {
        gain_.store(std::max(1.0, gain), std::memory_order_relaxed);
        gain_stability_.store(std::max(0.0, stability), std::memory_order_relaxed);
        noise_generator_.set_gain_variation(stability);
    }
    
    /**
     * @brief Set saturation level
     * @param saturation Maximum count rate before saturation
     */
    void set_saturation_level(double saturation) {
        saturation_level_.store(std::max(1e3, saturation), std::memory_order_relaxed);
    }
    
    /**
     * @brief Enable or disable saturation simulation
     * @param enable true to enable saturation effects
     */
    void enable_saturation(bool enable) {
        enable_saturation_.store(enable, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set operating temperature
     * @param temp_celsius Temperature in Celsius
     */
    void set_temperature(double temp_celsius) {
        temperature_.store(temp_celsius, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set calibration parameters
     * @param scale Calibration scale factor
     * @param offset Calibration offset
     */
    void set_calibration(double scale, double offset = 0.0) {
        calibration_scale_.store(scale, std::memory_order_relaxed);
        calibration_offset_.store(offset, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set conversion factor (e.g., counts/s to μA)
     * @param factor Conversion factor
     */
    void set_conversion_factor(double factor) {
        conversion_factor_.store(factor, std::memory_order_relaxed);
    }
    
    /**
     * @brief Enable or disable noise simulation
     * @param enable true to enable noise
     */
    void enable_noise(bool enable) {
        enable_noise_.store(enable, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set nonlinearity coefficient
     * @param error Fractional nonlinearity error
     */
    void set_linearity_error(double error) {
        linearity_error_.store(std::max(0.0, error), std::memory_order_relaxed);
    }
    
    /**
     * @brief Get current measurement statistics
     * @return tuple of (read_count, last_reading, signal_to_noise_ratio)
     */
    std::tuple<uint64_t, double, double> get_measurement_stats() const {
        uint64_t reads = read_count_.load(std::memory_order_relaxed);
        double last = last_reading_.load(std::memory_order_relaxed);
        
        // Estimate SNR based on current signal and dark current
        double signal = true_intensity_.load() * conversion_factor_.load();
        double dark = dark_current_.load() * conversion_factor_.load();
        double snr = (signal > 0) ? signal / std::sqrt(signal + dark) : 0.0;
        
        return {reads, last, snr};
    }
    
    /**
     * @brief Check if detector is saturated at current intensity
     */
    bool is_saturated() const {
        if (!enable_saturation_.load()) return false;
        
        double intensity = true_intensity_.load();
        double sat_level = saturation_level_.load();
        return intensity > sat_level * 0.9; // 90% of saturation
    }
    
    /**
     * @brief Perform self-test
     */
    bool self_test() override {
        if (!initialized_) return false;
        
        try {
            // Store original settings
            double orig_intensity = true_intensity_.load();
            bool orig_noise = enable_noise_.load();
            
            // Test with known intensity and no noise
            set_beam_intensity(1000.0);
            enable_noise(false);
            
            double reading1 = read();
            double reading2 = read();
            
            // Restore original settings
            set_beam_intensity(orig_intensity);
            enable_noise(orig_noise);
            
            // Readings should be consistent without noise
            double diff = std::abs(reading1 - reading2);
            double avg = (reading1 + reading2) / 2.0;
            double relative_diff = (avg > 0) ? diff / avg : diff;
            
            return relative_diff < 0.01; // <1% variation expected
            
        } catch (...) {
            return false;
        }
    }
    
    // ISensor interface implementations
    std::string get_type_name() const override { return "BIC"; }
    std::string get_units() const override { return "counts/s"; } // or μA, configurable
    std::pair<double, double> get_range() const override { 
        return {0.0, saturation_level_.load() * conversion_factor_.load()}; 
    }
    double get_resolution() const override { 
        // Resolution limited by counting statistics at 1% of full scale
        double full_scale = saturation_level_.load() * conversion_factor_.load();
        return full_scale * 0.01 / std::sqrt(full_scale * 0.01);
    }
};
