#pragma once
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <functional>

/**
 * @brief Beam Loss Monitor (BLM) for machine protection
 * 
 * Simulates radiation detectors around the beamline that monitor
 * for dangerous beam losses. Critical component of any accelerator
 * machine protection system.
 */
class BeamLossMonitor {
private:
    std::string blm_id_;
    std::atomic<double> loss_rate_{0.0};        ///< Current loss rate (Gy/s)
    std::atomic<double> threshold_warning_{1e-6}; ///< Warning threshold
    std::atomic<double> threshold_abort_{1e-5};   ///< Beam abort threshold
    std::atomic<bool> warning_active_{false};
    std::atomic<bool> abort_active_{false};
    
    // Statistics
    std::atomic<uint64_t> total_measurements_{0};
    std::atomic<uint64_t> warning_count_{0};
    std::atomic<uint64_t> abort_count_{0};
    
    // Callbacks for alarms
    std::function<void(const std::string&, double)> warning_callback_;
    std::function<void(const std::string&, double)> abort_callback_;

public:
    /**
     * @brief Construct BLM with ID and thresholds
     */
    explicit BeamLossMonitor(const std::string& id = "BLM_01") 
        : blm_id_(id) {}
    
    /**
     * @brief Update loss rate measurement
     * @param beam_current Current beam intensity
     * @param beam_position Beam position (affects loss rate)
     * @return true if measurement within safe limits
     */
    bool update_measurement(double beam_current, double beam_position) {
        total_measurements_.fetch_add(1);
        
        // Simulate loss rate based on beam conditions
        // Higher losses when beam is off-center or high current
        double position_factor = 1.0 + std::abs(beam_position) * 0.1;
        double current_factor = beam_current / 1000.0; // Normalize to typical current
        double base_loss = 1e-8; // Background loss rate
        
        double simulated_loss = base_loss * position_factor * current_factor;
        loss_rate_.store(simulated_loss);
        
        // Check thresholds
        bool warning_triggered = simulated_loss > threshold_warning_.load();
        bool abort_triggered = simulated_loss > threshold_abort_.load();
        
        if (abort_triggered && !abort_active_.load()) {
            abort_active_.store(true);
            abort_count_.fetch_add(1);
            if (abort_callback_) {
                abort_callback_(blm_id_, simulated_loss);
            }
            return false; // Unsafe condition
        }
        
        if (warning_triggered && !warning_active_.load()) {
            warning_active_.store(true);
            warning_count_.fetch_add(1);
            if (warning_callback_) {
                warning_callback_(blm_id_, simulated_loss);
            }
        }
        
        // Reset warnings if below threshold
        if (!warning_triggered) {
            warning_active_.store(false);
        }
        if (!abort_triggered) {
            abort_active_.store(false);
        }
        
        return true; // Safe condition
    }
    
    /**
     * @brief Set warning threshold
     */
    void set_warning_threshold(double threshold) {
        threshold_warning_.store(threshold);
    }
    
    /**
     * @brief Set abort threshold  
     */
    void set_abort_threshold(double threshold) {
        threshold_abort_.store(threshold);
    }
    
    /**
     * @brief Set warning callback
     */
    void set_warning_callback(std::function<void(const std::string&, double)> callback) {
        warning_callback_ = callback;
    }
    
    /**
     * @brief Set abort callback
     */
    void set_abort_callback(std::function<void(const std::string&, double)> callback) {
        abort_callback_ = callback;
    }
    
    /**
     * @brief Get current loss rate
     */
    double get_loss_rate() const {
        return loss_rate_.load();
    }
    
    /**
     * @brief Check if warning is active
     */
    bool is_warning_active() const {
        return warning_active_.load();
    }
    
    /**
     * @brief Check if abort condition is active
     */
    bool is_abort_active() const {
        return abort_active_.load();
    }
    
    /**
     * @brief Get BLM identifier
     */
    const std::string& get_id() const {
        return blm_id_;
    }
    
    /**
     * @brief Get statistics
     */
    struct Statistics {
        uint64_t total_measurements;
        uint64_t warning_count;
        uint64_t abort_count;
        double current_loss_rate;
        bool warning_active;
        bool abort_active;
    };
    
    Statistics get_statistics() const {
        return {
            total_measurements_.load(),
            warning_count_.load(),
            abort_count_.load(),
            loss_rate_.load(),
            warning_active_.load(),
            abort_active_.load()
        };
    }
    
    /**
     * @brief Reset all counters
     */
    void reset_statistics() {
        total_measurements_.store(0);
        warning_count_.store(0);
        abort_count_.store(0);
        warning_active_.store(false);
        abort_active_.store(false);
    }
};
