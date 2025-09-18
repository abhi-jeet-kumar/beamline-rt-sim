#pragma once
#include "beam_loss_monitor.hpp"
#include <vector>
#include <atomic>
#include <memory>
#include <string>
#include <chrono>

/**
 * @brief Machine Protection System (MPS)
 * 
 * Coordinates all safety systems to protect the accelerator and personnel.
 * Implements CERN-standard safety architecture with redundant monitoring
 * and automatic beam abort capabilities.
 */
class MachineProtectionSystem {
private:
    std::vector<std::unique_ptr<BeamLossMonitor>> blms_;
    std::atomic<bool> beam_permit_{true};
    std::atomic<bool> abort_triggered_{false};
    std::atomic<uint64_t> total_aborts_{0};
    
    // Callbacks for system-wide actions
    std::function<void()> beam_abort_callback_;
    std::function<void(const std::string&)> alarm_callback_;

public:
    /**
     * @brief Constructor
     */
    MachineProtectionSystem() {
        // Create standard BLM layout for beamline
        add_blm("BLM_UPSTREAM", -5.0);   // 5m upstream of target
        add_blm("BLM_TARGET", 0.0);      // At interaction point  
        add_blm("BLM_DOWNSTREAM", 5.0);  // 5m downstream
    }
    
    /**
     * @brief Add a beam loss monitor
     */
    void add_blm(const std::string& id, double position) {
        auto blm = std::make_unique<BeamLossMonitor>(id);
        
        // Set up callbacks for this BLM
        blm->set_warning_callback([this, id](const std::string& blm_id, double loss_rate) {
            handle_blm_warning(blm_id, loss_rate);
        });
        
        blm->set_abort_callback([this, id](const std::string& blm_id, double loss_rate) {
            handle_blm_abort(blm_id, loss_rate);
        });
        
        blms_.push_back(std::move(blm));
    }
    
    /**
     * @brief Update MPS with current beam conditions
     */
    bool check_safety(double beam_current, double beam_position) {
        if (!beam_permit_.load()) {
            return false; // No beam permit
        }
        
        if (abort_triggered_.load()) {
            return false; // Already in abort state
        }
        
        // Check all BLMs
        bool all_safe = true;
        for (auto& blm : blms_) {
            // Simulate position-dependent loss rates
            double local_position = beam_position; // Simplified - same for all BLMs
            bool blm_safe = blm->update_measurement(beam_current, local_position);
            
            if (!blm_safe) {
                all_safe = false;
                trigger_beam_abort("BLM_ABORT", blm->get_id());
                break;
            }
        }
        
        return all_safe;
    }
    
    /**
     * @brief Trigger beam abort
     */
    void trigger_beam_abort(const std::string& reason, const std::string& source = "") {
        abort_triggered_.store(true);
        beam_permit_.store(false);
        total_aborts_.fetch_add(1);
        
        if (beam_abort_callback_) {
            beam_abort_callback_();
        }
        
        if (alarm_callback_) {
            std::string message = "BEAM ABORT: " + reason;
            if (!source.empty()) {
                message += " (Source: " + source + ")";
            }
            alarm_callback_(message);
        }
    }
    
    /**
     * @brief Reset MPS to operational state
     */
    void reset_mps() {
        abort_triggered_.store(false);
        beam_permit_.store(true);
        
        // Reset all BLMs
        for (auto& blm : blms_) {
            blm->reset_statistics();
        }
    }
    
    /**
     * @brief Set beam abort callback
     */
    void set_beam_abort_callback(std::function<void()> callback) {
        beam_abort_callback_ = callback;
    }
    
    /**
     * @brief Set alarm callback
     */
    void set_alarm_callback(std::function<void(const std::string&)> callback) {
        alarm_callback_ = callback;
    }
    
    /**
     * @brief Check if beam is permitted
     */
    bool is_beam_permitted() const {
        return beam_permit_.load() && !abort_triggered_.load();
    }
    
    /**
     * @brief Check if abort is active
     */
    bool is_abort_active() const {
        return abort_triggered_.load();
    }
    
    /**
     * @brief Get total abort count
     */
    uint64_t get_abort_count() const {
        return total_aborts_.load();
    }
    
    /**
     * @brief Get BLM by ID
     */
    BeamLossMonitor* get_blm(const std::string& id) {
        for (auto& blm : blms_) {
            if (blm->get_id() == id) {
                return blm.get();
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get all BLM statistics
     */
    std::vector<BeamLossMonitor::Statistics> get_all_blm_stats() const {
        std::vector<BeamLossMonitor::Statistics> stats;
        for (const auto& blm : blms_) {
            stats.push_back(blm->get_statistics());
        }
        return stats;
    }

private:
    void handle_blm_warning(const std::string& blm_id, double loss_rate) {
        if (alarm_callback_) {
            alarm_callback_("BLM WARNING: " + blm_id + " loss rate: " + std::to_string(loss_rate));
        }
    }
    
    void handle_blm_abort(const std::string& blm_id, double loss_rate) {
        trigger_beam_abort("BLM_THRESHOLD_EXCEEDED", blm_id);
    }
};
