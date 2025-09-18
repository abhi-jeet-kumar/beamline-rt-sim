#pragma once
#include "../core/clock.hpp"
#include "../core/pid.hpp"
#include "../core/telemetry.hpp"
#include "../core/watchdog.hpp"
#include "../control/api.hpp"
#include "../control/limits.hpp"
#include "../hw/bpm.hpp"
#include "../hw/magnet.hpp"
#include <atomic>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Real-time control loop with PID feedback and telemetry
 * 
 * Implements the main control loop running at configurable frequency (default 1kHz).
 * Handles sensor readout, PID control calculation, actuator updates, and telemetry
 * publishing while maintaining strict timing constraints.
 * 
 * Features:
 * - Configurable loop frequency (10-2000 Hz)
 * - Watchdog-based deadline monitoring
 * - JSON-based command interface
 * - Real-time telemetry publishing
 * - Safe startup and emergency stop
 */
class RTLoop {
private:
    ControlAPI& api_;
    BPM& bpm_ref_;
    IActuator& magnet_ref_;
    PID pid_;
    Limits limits_;
    
    std::atomic<bool> running_{true};
    std::atomic<double> loop_frequency_{1000.0};  // Hz
    std::atomic<uint64_t> loop_count_{0};
    std::atomic<uint64_t> deadline_misses_{0};
    
    // Performance tracking
    mutable std::atomic<double> avg_loop_time_{0.0};
    mutable std::atomic<double> max_loop_time_{0.0};
    mutable std::atomic<double> last_loop_time_{0.0};
    
    // Control state
    std::atomic<bool> control_enabled_{true};
    std::atomic<bool> emergency_stop_{false};
    
public:
    /**
     * @brief Construct real-time control loop
     * @param api Reference to control API
     * @param bpm_ref Reference to BPM for direct control
     * @param magnet_ref Reference to magnet for direct control
     */
    RTLoop(ControlAPI& api, BPM& bpm_ref, IActuator& magnet_ref)
        : api_(api), bpm_ref_(bpm_ref), magnet_ref_(magnet_ref) {
        
        // Initialize PID with conservative gains
        pid_.kp = 0.6;
        pid_.ki = 0.05;
        pid_.kd = 0.0;
        pid_.setpoint = 0.0;
        
        // Set integrator limits to prevent windup
        pid_.set_integrator_limits(-10.0, 10.0);
    }
    
    /**
     * @brief Main control loop execution
     * @param telemetry_pub Telemetry publisher
     * @param control_rep Control command responder
     */
    template<typename TelemetryPub, typename ControlRep>
    void run(TelemetryPub& telemetry_pub, ControlRep& control_rep) {
        // Calculate initial period
        auto period_ns = std::chrono::nanoseconds(
            static_cast<long long>(1e9 / loop_frequency_.load()));
        
        PeriodicClock clock(period_ns);
        Watchdog watchdog(period_ns);
        
        auto start_time = std::chrono::steady_clock::now();
        double running_avg = 0.0;
        double max_time = 0.0;
        
        while (running_.load(std::memory_order_relaxed)) {
            auto loop_start = std::chrono::steady_clock::now();
            
            try {
                // Read sensors
                double beam_position = api_.read_beam_position();
                double beam_intensity = api_.read_beam_intensity();
                
                // Check sensor readings are reasonable
                if (!limits_.is_bpm_position_reasonable(beam_position)) {
                    // Log warning but continue
                }
                if (!limits_.is_bic_intensity_reasonable(beam_intensity)) {
                    // Log warning but continue  
                }
                
                // Control calculation
                double magnet_output = 0.0;
                if (control_enabled_.load() && !emergency_stop_.load()) {
                    double dt = std::chrono::duration<double>(period_ns).count();
                    magnet_output = pid_.step(beam_position, dt, 
                                            limits_.magnet_min, limits_.magnet_max);
                    
                    // Apply additional safety clamping
                    magnet_output = limits_.clamp_magnet_current(magnet_output);
                    
                    // Set magnet current
                    api_.set_magnet_current(magnet_output);
                    
                    // Simulate magnet influence on beam position
                    // This creates the closed-loop physics simulation
                    if (auto* bpm_ptr = dynamic_cast<BPM*>(&bpm_ref_)) {
                        auto [current_x, current_y] = bpm_ptr->get_beam_position();
                        // Magnet affects X position with some coupling
                        double new_x = current_x - 0.4 * magnet_output;
                        bpm_ptr->set_beam_position(new_x, current_y);
                    }
                } else {
                    // Control disabled or emergency stop - hold magnet at zero
                    api_.set_magnet_current(0.0);
                }
                
                // Check timing constraint
                auto loop_end = std::chrono::steady_clock::now();
                watchdog.check(loop_start, loop_end);
                
                // Update performance statistics
                double loop_time = std::chrono::duration<double>(loop_end - loop_start).count() * 1000.0; // ms
                last_loop_time_.store(loop_time);
                if (loop_time > max_time) {
                    max_time = loop_time;
                    max_loop_time_.store(max_time);
                }
                
                // Update running average
                uint64_t count = loop_count_.load();
                running_avg = (running_avg * count + loop_time) / (count + 1);
                avg_loop_time_.store(running_avg);
                
                // Publish telemetry
                double elapsed_time = std::chrono::duration<double>(loop_end - start_time).count();
                json telemetry = {
                    {"t", elapsed_time},
                    {"pos", beam_position},
                    {"intensity", beam_intensity},
                    {"mag", api_.get_magnet_current()},
                    {"deadline_miss", watchdog.is_tripped() ? 1 : 0},
                    {"loop_time_ms", loop_time},
                    {"pid_error", pid_.get_error()},
                    {"pid_p", pid_.get_proportional()},
                    {"pid_i", pid_.get_integral()},
                    {"pid_d", pid_.get_derivative()},
                    {"control_enabled", control_enabled_.load()},
                    {"emergency_stop", emergency_stop_.load()}
                };
                
                telemetry_pub.send(telemetry.dump());
                
                // Count deadline misses
                if (watchdog.is_tripped()) {
                    deadline_misses_.fetch_add(1);
                    
                    // Auto-reduce frequency if too many misses
                    if (deadline_misses_.load() % 10 == 0) {
                        double current_freq = loop_frequency_.load();
                        double new_freq = current_freq * 0.8;  // Reduce by 20%
                        new_freq = limits_.clamp_frequency(new_freq);
                        
                        if (new_freq != current_freq) {
                            loop_frequency_.store(new_freq);
                            period_ns = std::chrono::nanoseconds(
                                static_cast<long long>(1e9 / new_freq));
                            clock.set_period(period_ns);
                            watchdog.set_budget(period_ns);
                            
                            // Send alarm telemetry
                            json alarm = {
                                {"type", "frequency_reduced"},
                                {"old_freq", current_freq},
                                {"new_freq", new_freq},
                                {"reason", "deadline_misses"}
                            };
                            telemetry_pub.send_with_topic("alarm", alarm.dump());
                        }
                    }
                }
                
                // Handle control commands (non-blocking)
                if (control_rep.has_request()) {
                    std::string command = control_rep.recv_timeout(1); // 1ms timeout
                    if (!command.empty()) {
                        std::string response = handle_command(command);
                        control_rep.reply(response);
                        
                        // Update clock period if frequency changed
                        auto new_period_ns = std::chrono::nanoseconds(
                            static_cast<long long>(1e9 / loop_frequency_.load()));
                        if (new_period_ns != period_ns) {
                            period_ns = new_period_ns;
                            clock.set_period(period_ns);
                            watchdog.set_budget(period_ns);
                        }
                    }
                }
                
                loop_count_.fetch_add(1);
                
            } catch (const std::exception& e) {
                // Log error and continue
                json error_telemetry = {
                    {"type", "loop_error"},
                    {"error", e.what()},
                    {"loop_count", loop_count_.load()}
                };
                telemetry_pub.send_with_topic("error", error_telemetry.dump());
                
                // Emergency stop on critical errors
                emergency_stop_.store(true);
                api_.emergency_stop();
            }
            
            // Wait for next cycle
            clock.wait_next();
            watchdog.reset();
        }
        
        // Final cleanup
        api_.emergency_stop();
        json shutdown = {{"type", "shutdown"}, {"loop_count", loop_count_.load()}};
        telemetry_pub.send_with_topic("status", shutdown.dump());
    }
    
    /**
     * @brief Handle incoming JSON command
     * @param command_json JSON command string
     * @param period_ns Reference to current period (may be modified)
     * @return JSON response string
     */
    std::string handle_command(const std::string& command_json) {
        try {
            auto command = json::parse(command_json);
            
            if (!command.is_object() || !command.contains("cmd")) {
                return R"({"ok": false, "error": "Invalid command format"})";
            }
            
            std::string cmd = command["cmd"];
            
            if (cmd == "set_pid") {
                auto [kp, ki, kd] = limits_.clamp_pid_gains(
                    command.value("kp", pid_.kp),
                    command.value("ki", pid_.ki), 
                    command.value("kd", pid_.kd)
                );
                
                pid_.kp = kp;
                pid_.ki = ki;
                pid_.kd = kd;
                
                return R"({"ok": true, "message": "PID gains updated"})";
                
            } else if (cmd == "set_freq") {
                double new_freq = limits_.clamp_frequency(command.value("hz", loop_frequency_.load()));
                loop_frequency_.store(new_freq);
                
                return R"({"ok": true, "message": "Frequency updated"})";
                
            } else if (cmd == "set_setpoint") {
                double sp = command.value("sp", 0.0);
                pid_.set_setpoint(sp);
                
                return R"({"ok": true, "message": "Setpoint updated"})";
                
            } else if (cmd == "recommission") {
                // Reset PID state
                pid_.reset();
                pid_.setpoint = 0.0;
                
                // Reset magnet
                api_.emergency_stop();
                
                // Reset beam position if possible
                if (auto* bpm_ptr = dynamic_cast<BPM*>(&bpm_ref_)) {
                    bpm_ptr->set_beam_position(0.0, 0.0);
                }
                
                // Clear emergency stop and enable control
                emergency_stop_.store(false);
                control_enabled_.store(true);
                
                // Reset statistics
                deadline_misses_.store(0);
                loop_count_.store(0);
                
                return R"({"ok": true, "message": "System recommissioned"})";
                
            } else if (cmd == "emergency_stop") {
                emergency_stop_.store(true);
                api_.emergency_stop();
                
                return R"({"ok": true, "message": "Emergency stop activated"})";
                
            } else if (cmd == "enable_control") {
                bool enable = command.value("enable", true);
                control_enabled_.store(enable);
                
                if (!enable) {
                    api_.emergency_stop();
                }
                
                return R"({"ok": true, "message": "Control enable updated"})";
                
            } else if (cmd == "get_status") {
                json status = {
                    {"ok", true},
                    {"loop_frequency", loop_frequency_.load()},
                    {"loop_count", loop_count_.load()},
                    {"deadline_misses", deadline_misses_.load()},
                    {"avg_loop_time_ms", avg_loop_time_.load()},
                    {"max_loop_time_ms", max_loop_time_.load()},
                    {"control_enabled", control_enabled_.load()},
                    {"emergency_stop", emergency_stop_.load()},
                    {"pid_gains", {{"kp", pid_.kp}, {"ki", pid_.ki}, {"kd", pid_.kd}}},
                    {"setpoint", pid_.setpoint}
                };
                
                return status.dump();
                
            } else if (cmd == "stop") {
                running_.store(false);
                return R"({"ok": true, "message": "Stopping control loop"})";
            }
            
            return R"({"ok": false, "error": "Unknown command"})";
            
        } catch (const json::exception& e) {
            return R"({"ok": false, "error": "JSON parse error"})";
        } catch (const std::exception& e) {
            return R"({"ok": false, "error": "Command execution error"})";
        }
    }
    
    /**
     * @brief Stop the control loop
     */
    void stop() {
        running_.store(false);
    }
    
    /**
     * @brief Get current loop statistics
     */
    struct LoopStats {
        uint64_t loop_count;
        uint64_t deadline_misses;
        double avg_loop_time_ms;
        double max_loop_time_ms;
        double last_loop_time_ms;
        double frequency_hz;
    };
    
    LoopStats get_stats() const {
        return {
            loop_count_.load(),
            deadline_misses_.load(), 
            avg_loop_time_.load(),
            max_loop_time_.load(),
            last_loop_time_.load(),
            loop_frequency_.load()
        };
    }
};
