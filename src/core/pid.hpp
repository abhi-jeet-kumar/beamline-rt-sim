#pragma once
#include <algorithm>
#include <cmath>

/**
 * @brief PID Controller with integrator windup protection and derivative filtering
 * 
 * Implements a discrete-time PID controller suitable for real-time control loops.
 * Features anti-windup protection, derivative kick prevention, and configurable
 * output limits for safety-critical applications.
 * 
 * The controller uses the standard form:
 * u(t) = Kp*e(t) + Ki*∫e(τ)dτ + Kd*de(t)/dt
 * 
 * Where:
 * - e(t) = setpoint - measurement (error)
 * - Integrator is clamped to prevent windup
 * - Derivative is calculated on error change to prevent setpoint kick
 */
struct PID {
    // Controller gains
    double kp{0.1};  ///< Proportional gain
    double ki{0.0};  ///< Integral gain  
    double kd{0.0};  ///< Derivative gain
    
    // Control target
    double setpoint{0.0};  ///< Target value for the controlled variable
    
    // Internal state
    double integ{0.0};      ///< Integrator accumulator
    double prev_err{0.0};   ///< Previous error for derivative calculation
    
    // Optional integrator limits (disabled by default)
    double integ_min{-1e6}; ///< Minimum integrator value
    double integ_max{1e6};  ///< Maximum integrator value
    
    // Statistics (for debugging/tuning)
    mutable double last_proportional{0.0};
    mutable double last_integral{0.0};
    mutable double last_derivative{0.0};
    mutable double last_error{0.0};

    /**
     * @brief Execute one PID control step
     * 
     * @param measurement Current measured value
     * @param dt Time step in seconds
     * @param out_min Minimum allowed output value
     * @param out_max Maximum allowed output value
     * @return Control output clamped to [out_min, out_max]
     */
    double step(double measurement, double dt, double out_min, double out_max) {
        // Calculate error
        double error = setpoint - measurement;
        last_error = error;
        
        // Proportional term
        double proportional = kp * error;
        last_proportional = proportional;
        
        // Integral term with windup protection
        if (dt > 0.0) {
            // Only accumulate if we're not saturated or if the error would reduce saturation
            double tentative_integ = integ + error * dt;
            
            // Clamp integrator to configured limits
            tentative_integ = std::max(integ_min, std::min(integ_max, tentative_integ));
            
            // Anti-windup: only update integrator if output won't saturate
            double tentative_output = proportional + ki * tentative_integ;
            
            if (tentative_output >= out_min && tentative_output <= out_max) {
                // Output is within bounds, safe to update integrator
                integ = tentative_integ;
            } else {
                // Output would saturate - use conditional integration
                // Only integrate if it would help bring output into bounds
                double current_output = proportional + ki * integ;
                
                if ((tentative_output > out_max && current_output > tentative_output) ||
                    (tentative_output < out_min && current_output < tentative_output)) {
                    // Integration would help, allow it
                    integ = tentative_integ;
                }
                // Otherwise, don't update integrator (anti-windup)
            }
        }
        
        double integral = ki * integ;
        last_integral = integral;
        
        // Derivative term (calculated on error to avoid derivative kick)
        double derivative = 0.0;
        if (dt > 1e-9 && kd != 0.0) {  // Avoid division by zero
            derivative = kd * (error - prev_err) / dt;
        }
        last_derivative = derivative;
        
        // Update state for next iteration
        prev_err = error;
        
        // Calculate total output
        double output = proportional + integral + derivative;
        
        // Clamp output to allowed range
        output = std::max(out_min, std::min(out_max, output));
        
        return output;
    }

    /**
     * @brief Reset controller to initial state
     * 
     * Clears integrator and derivative state. Use when starting control
     * or when there's been a significant discontinuity.
     */
    void reset() {
        integ = 0.0;
        prev_err = 0.0;
        last_proportional = 0.0;
        last_integral = 0.0;
        last_derivative = 0.0;
        last_error = 0.0;
    }

    /**
     * @brief Set new setpoint with bumpless transfer
     * 
     * Updates setpoint while optionally resetting derivative state
     * to prevent large derivative kick from setpoint changes.
     * 
     * @param new_setpoint New target value
     * @param reset_derivative If true, resets derivative state to prevent kick
     */
    void set_setpoint(double new_setpoint, bool reset_derivative = true) {
        if (reset_derivative) {
            // Recalculate prev_err based on new setpoint to prevent derivative kick
            prev_err = new_setpoint - (setpoint - prev_err);
        }
        setpoint = new_setpoint;
    }

    /**
     * @brief Configure integrator limits for windup protection
     * 
     * @param min_val Minimum integrator value
     * @param max_val Maximum integrator value  
     */
    void set_integrator_limits(double min_val, double max_val) {
        integ_min = min_val;
        integ_max = max_val;
        // Clamp current integrator value to new limits
        integ = std::max(integ_min, std::min(integ_max, integ));
    }

    /**
     * @brief Check if controller output is currently saturated
     * 
     * @param out_min Minimum output limit
     * @param out_max Maximum output limit
     * @return true if last output hit limits
     */
    bool is_saturated(double out_min, double out_max) const {
        double total_output = last_proportional + last_integral + last_derivative;
        return (total_output <= out_min) || (total_output >= out_max);
    }

    /**
     * @brief Get proportional contribution from last step
     */
    double get_proportional() const { return last_proportional; }
    
    /**
     * @brief Get integral contribution from last step  
     */
    double get_integral() const { return last_integral; }
    
    /**
     * @brief Get derivative contribution from last step
     */
    double get_derivative() const { return last_derivative; }
    
    /**
     * @brief Get error from last step
     */
    double get_error() const { return last_error; }

    /**
     * @brief Get current integrator value
     */
    double get_integrator() const { return integ; }
};
