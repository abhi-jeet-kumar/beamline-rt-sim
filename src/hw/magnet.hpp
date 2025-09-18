#pragma once
#include "iactuator.hpp"
#include "sim_noise.hpp"
#include <chrono>
#include <atomic>
#include <cmath>
#include <thread>

/**
 * @brief Steering Magnet simulation
 * 
 * Simulates a beam steering electromagnet with realistic physics:
 * - Current-to-field conversion with magnetic saturation
 * - Power supply regulation and ripple
 * - Thermal effects and drift
 * - Hysteresis and eddy current effects
 * - Safety interlocks and current limiting
 * - Slew rate limiting for magnet protection
 * 
 * Designed for high-frequency control updates with realistic magnetic modeling.
 */
class Magnet : public IActuator {
private:
    // Magnet physical parameters
    std::atomic<double> current_setpoint_{0.0};      ///< Commanded current (A)
    std::atomic<double> actual_current_{0.0};        ///< Actual magnet current (A)
    std::atomic<double> magnetic_field_{0.0};        ///< Resulting magnetic field (T)
    
    // Magnet characteristics
    std::atomic<double> inductance_{0.1};            ///< Magnet inductance (H)
    std::atomic<double> resistance_{1.0};            ///< Magnet resistance (Ω)
    std::atomic<double> field_constant_{0.01};       ///< Current to field conversion (T/A)
    std::atomic<double> saturation_current_{100.0};  ///< Saturation current (A)
    std::atomic<double> saturation_field_{1.0};      ///< Saturation field (T)
    
    // Power supply characteristics
    std::atomic<double> max_voltage_{100.0};         ///< Maximum supply voltage (V)
    std::atomic<double> regulation_stability_{1e-5}; ///< Current regulation stability
    std::atomic<double> ripple_amplitude_{1e-4};     ///< Power supply ripple amplitude
    std::atomic<double> slew_rate_limit_{10.0};      ///< Maximum di/dt (A/s)
    
    // Environmental effects
    std::atomic<double> temperature_{20.0};          ///< Operating temperature (°C)
    std::atomic<double> temp_coefficient_{0.001};    ///< Resistance temp coeff (/°C)
    std::atomic<double> field_temp_coeff_{0.0001};   ///< Field temp coefficient (/°C)
    
    // Hysteresis and dynamic effects
    std::atomic<double> hysteresis_width_{0.01};     ///< Hysteresis loop width (A)
    std::atomic<double> eddy_current_time_{0.001};   ///< Eddy current time constant (s)
    mutable double previous_current_{0.0};           ///< For hysteresis calculation
    mutable double eddy_current_field_{0.0};         ///< Eddy current contribution
    
    // Noise simulation
    mutable BeamlineNoise::MagnetNoise noise_generator_;
    std::atomic<bool> enable_noise_{true};
    
    // Control system state
    std::atomic<double> current_ramp_rate_{0.0};     ///< Current ramping rate (A/s)
    mutable std::chrono::steady_clock::time_point last_update_time_;
    
    // Safety systems
    std::atomic<bool> interlock_active_{false};      ///< Safety interlock status
    std::atomic<double> quench_threshold_{90.0};     ///< Quench detection threshold (% of max)
    std::atomic<bool> emergency_stop_active_{false}; ///< Emergency stop status
    
    // Performance tracking
    mutable std::atomic<uint64_t> command_count_{0};
    mutable std::atomic<double> total_energy_dissipated_{0.0}; ///< Total energy (J)
    
public:
    /**
     * @brief Construct Magnet with specified ID
     * @param magnet_id Unique identifier for this magnet
     * @param noise_seed Random seed for noise generation (0 = random)
     */
    explicit Magnet(const std::string& magnet_id = "MAG_01", uint64_t noise_seed = 0)
        : noise_generator_(noise_seed)
        , last_update_time_(std::chrono::steady_clock::now())
    {
        set_id(magnet_id);
        set_limits(-50.0, 50.0);  // Default ±50A range
        set_rate_limit(10.0);     // Default 10 A/s slew rate
    }
    
    /**
     * @brief Set magnet current (implements IActuator::set)
     * @param current_amps Target current in Amperes
     */
    void set(double current_amps) override {
        if (emergency_stop_active_.load(std::memory_order_relaxed)) {
            throw std::runtime_error("Magnet emergency stop active");
        }
        
        if (interlock_active_.load(std::memory_order_relaxed)) {
            throw std::runtime_error("Magnet safety interlock active");
        }
        
        command_count_.fetch_add(1, std::memory_order_relaxed);
        
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_update_time_).count();
        last_update_time_ = now;
        
        // Apply slew rate limiting
        double current_actual = actual_current_.load(std::memory_order_relaxed);
        double max_change = slew_rate_limit_.load() * dt;
        double requested_change = current_amps - current_actual;
        
        if (std::abs(requested_change) > max_change) {
            // Limit the change rate
            double limited_change = (requested_change > 0) ? max_change : -max_change;
            current_amps = current_actual + limited_change;
            current_ramp_rate_.store(limited_change / dt, std::memory_order_relaxed);
        } else {
            current_ramp_rate_.store(0.0, std::memory_order_relaxed);
        }
        
        // Store setpoint
        current_setpoint_.store(current_amps, std::memory_order_relaxed);
        
        // Simulate power supply response with L/R time constant
        double L = inductance_.load(std::memory_order_relaxed);
        double R = resistance_.load(std::memory_order_relaxed);
        double time_constant = L / R;
        
        // First-order response for current settling
        double alpha = dt / (time_constant + dt);
        double new_current = alpha * current_amps + (1.0 - alpha) * current_actual;
        
        // Add power supply noise
        if (enable_noise_.load(std::memory_order_relaxed)) {
            double current_noise = noise_generator_.generate(new_current, dt);
            new_current += current_noise;
        }
        
        // Check for quench condition (simplified)
        double current_fraction = std::abs(new_current) / saturation_current_.load();
        if (current_fraction > quench_threshold_.load() / 100.0) {
            interlock_active_.store(true, std::memory_order_relaxed);
            new_current = 0.0; // Quench protection - dump current
        }
        
        actual_current_.store(new_current, std::memory_order_relaxed);
        
        // Calculate magnetic field with saturation
        update_magnetic_field();
        
        // Update energy dissipation
        double power = new_current * new_current * get_effective_resistance();
        double energy = power * dt;
        total_energy_dissipated_.fetch_add(energy, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get current magnet current (implements IActuator::get)
     * @return Actual current in Amperes
     */
    double get() const override {
        return actual_current_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Initialize magnet hardware simulation
     */
    bool initialize() override {
        if (!IActuator::initialize()) {
            return false;
        }
        
        // Reset state
        current_setpoint_.store(0.0, std::memory_order_relaxed);
        actual_current_.store(0.0, std::memory_order_relaxed);
        magnetic_field_.store(0.0, std::memory_order_relaxed);
        interlock_active_.store(false, std::memory_order_relaxed);
        emergency_stop_active_.store(false, std::memory_order_relaxed);
        command_count_.store(0, std::memory_order_relaxed);
        total_energy_dissipated_.store(0.0, std::memory_order_relaxed);
        
        last_update_time_ = std::chrono::steady_clock::now();
        
        return true;
    }
    
    /**
     * @brief Emergency stop - immediately set current to zero
     */
    void emergency_stop() override {
        emergency_stop_active_.store(true, std::memory_order_relaxed);
        actual_current_.store(0.0, std::memory_order_relaxed);
        current_setpoint_.store(0.0, std::memory_order_relaxed);
        magnetic_field_.store(0.0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Reset emergency stop condition
     */
    void reset_emergency_stop() {
        emergency_stop_active_.store(false, std::memory_order_relaxed);
    }
    
    /**
     * @brief Reset safety interlock
     */
    void reset_interlock() {
        interlock_active_.store(false, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get magnetic field strength
     * @return Magnetic field in Tesla
     */
    double get_magnetic_field() const {
        return magnetic_field_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Set magnet physical parameters
     * @param inductance_H Inductance in Henries
     * @param resistance_ohm Resistance in Ohms
     * @param field_constant_T_per_A Field constant in T/A
     */
    void set_magnet_parameters(double inductance_H, double resistance_ohm, double field_constant_T_per_A) {
        inductance_.store(std::max(1e-6, inductance_H), std::memory_order_relaxed);
        resistance_.store(std::max(1e-3, resistance_ohm), std::memory_order_relaxed);
        field_constant_.store(field_constant_T_per_A, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set saturation characteristics
     * @param sat_current_A Saturation current in Amperes
     * @param sat_field_T Saturation field in Tesla
     */
    void set_saturation(double sat_current_A, double sat_field_T) {
        saturation_current_.store(std::max(1.0, sat_current_A), std::memory_order_relaxed);
        saturation_field_.store(std::max(0.01, sat_field_T), std::memory_order_relaxed);
    }
    
    /**
     * @brief Set power supply characteristics
     * @param max_voltage_V Maximum voltage
     * @param regulation_ppm Current regulation in ppm
     * @param ripple_fraction Ripple as fraction of current
     */
    void set_power_supply(double max_voltage_V, double regulation_ppm = 10.0, double ripple_fraction = 1e-4) {
        max_voltage_.store(std::max(1.0, max_voltage_V), std::memory_order_relaxed);
        regulation_stability_.store(regulation_ppm * 1e-6, std::memory_order_relaxed);
        ripple_amplitude_.store(ripple_fraction, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set slew rate limit
     * @param rate_A_per_s Maximum current change rate in A/s
     */
    void set_slew_rate_limit(double rate_A_per_s) {
        slew_rate_limit_.store(std::max(0.1, rate_A_per_s), std::memory_order_relaxed);
        set_rate_limit(rate_A_per_s); // Update base class limit too
    }
    
    /**
     * @brief Set operating temperature
     * @param temp_celsius Temperature in Celsius
     */
    void set_temperature(double temp_celsius) {
        temperature_.store(temp_celsius, std::memory_order_relaxed);
    }
    
    /**
     * @brief Set hysteresis characteristics
     * @param width_A Hysteresis loop width in Amperes
     */
    void set_hysteresis(double width_A) {
        hysteresis_width_.store(std::max(0.0, width_A), std::memory_order_relaxed);
    }
    
    /**
     * @brief Enable or disable noise simulation
     * @param enable true to enable noise
     */
    void enable_noise(bool enable) {
        enable_noise_.store(enable, std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if magnet is ramping
     * @return true if current is actively changing
     */
    bool is_ramping() const {
        return std::abs(current_ramp_rate_.load()) > 0.01; // >0.01 A/s
    }
    
    /**
     * @brief Get current ramp rate
     * @return Current change rate in A/s
     */
    double get_ramp_rate() const {
        return current_ramp_rate_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check interlock status
     * @return true if safety interlock is active
     */
    bool is_interlock_active() const {
        return interlock_active_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total energy dissipated
     * @return Total energy in Joules
     */
    double get_total_energy_dissipated() const {
        return total_energy_dissipated_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get magnet power consumption
     * @return Current power in Watts
     */
    double get_power() const {
        double current = actual_current_.load();
        double resistance = get_effective_resistance();
        return current * current * resistance;
    }
    
    /**
     * @brief Get time constant for current settling
     * @return L/R time constant in seconds
     */
    double get_time_constant() const {
        double L = inductance_.load();
        double R = get_effective_resistance();
        return L / R;
    }
    
    /**
     * @brief Perform self-test
     */
    bool self_test() override {
        if (!initialized_) return false;
        
        try {
            // Store original state
            double orig_current = actual_current_.load();
            bool orig_noise = enable_noise_.load();
            bool orig_interlock = interlock_active_.load();
            
            // Reset for test
            reset_interlock();
            enable_noise(false);
            
            // Test small current change
            set(1.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            double current1 = get();
            
            set(0.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            double current2 = get();
            
            // Restore original state
            enable_noise(orig_noise);
            interlock_active_.store(orig_interlock, std::memory_order_relaxed);
            set(orig_current);
            
            // Verify magnet responds to commands
            return (std::abs(current1 - 1.0) < 0.5) && (std::abs(current2) < 0.1);
            
        } catch (...) {
            return false;
        }
    }
    
    // IActuator interface implementations
    std::string get_type_name() const override { return "Magnet"; }
    std::string get_units() const override { return "A"; }
    double get_resolution() const override { return 0.001; } // 1 mA resolution
    
private:
    /**
     * @brief Calculate effective resistance including temperature effects
     */
    double get_effective_resistance() const {
        double base_R = resistance_.load();
        double temp = temperature_.load();
        double temp_coeff = temp_coefficient_.load();
        return base_R * (1.0 + (temp - 20.0) * temp_coeff);
    }
    
    /**
     * @brief Update magnetic field based on current with saturation and hysteresis
     */
    void update_magnetic_field() {
        double current = actual_current_.load();
        double field_const = field_constant_.load();
        double sat_current = saturation_current_.load();
        double sat_field = saturation_field_.load();
        
        // Calculate ideal linear field
        double linear_field = current * field_const;
        
        // Apply saturation (tanh model)
        double normalized_current = current / sat_current;
        double saturation_factor = std::tanh(normalized_current);
        double saturated_field = sat_field * saturation_factor;
        
        // Choose between linear and saturated based on current level
        double field;
        if (std::abs(current) < sat_current * 0.9) {
            field = linear_field; // Linear region
        } else {
            field = saturated_field; // Saturated region
        }
        
        // Apply hysteresis (simplified model)
        double hysteresis = hysteresis_width_.load();
        if (hysteresis > 0.0) {
            double current_change = current - previous_current_;
            if (current_change > 0) {
                field -= hysteresis * 0.5; // Ascending branch
            } else if (current_change < 0) {
                field += hysteresis * 0.5; // Descending branch
            }
        }
        previous_current_ = current;
        
        // Apply temperature coefficient to field
        double temp = temperature_.load();
        double field_temp_coeff = field_temp_coeff_.load();
        field *= (1.0 + (temp - 20.0) * field_temp_coeff);
        
        magnetic_field_.store(field, std::memory_order_relaxed);
    }
};
