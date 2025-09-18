#pragma once
#include "../core/clock.hpp"
#include "../core/pid.hpp"
#include "../core/telemetry.hpp"
#include "../core/watchdog.hpp"
#include "../control/api.hpp"
#include "../control/limits.hpp"
#include "../hw/simple_bpm.hpp"
#include "../hw/simple_magnet.hpp"
#include "../safety/machine_protection_system.hpp"
#include <atomic>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <zmq.h>
#include <nlohmann/json.hpp> // header-only JSON (add to third_party or vendor)
using json = nlohmann::json;

/**
 * @brief Real-time control loop implementation
 * 
 * Implements the main control loop exactly as specified in technical_details.txt
 * - 1000 Hz default frequency
 * - PID feedback control with magnet-BPM coupling
 * - Watchdog timing enforcement
 * - JSON telemetry publishing  
 * - Non-blocking command handling
 */
struct RTLoop {
  ControlAPI& api;        ///< Control API reference
  BPM& bpm_ref;          ///< BPM reference for direct access
  Magnet& magnet_ref;    ///< Magnet reference for direct access  
  PID pid;               ///< PID controller instance
  Limits lim;            ///< Safety limits
  std::atomic<bool> running{true};  ///< Loop running flag
  double hz{1000.0};     ///< Loop frequency in Hz
  
  // Additional state for GUI integration
  std::atomic<bool> control_enabled{true};   ///< Control enable/disable
  std::atomic<bool> emergency_stop{false};   ///< Emergency stop state
  std::atomic<uint64_t> loop_count{0};       ///< Loop iteration counter
  std::atomic<uint64_t> deadline_misses{0};  ///< Deadline miss counter
  
  // Machine Protection System
  MachineProtectionSystem mps;

  /**
   * @brief Constructor
   * @param a ControlAPI reference
   * @param bpm BPM reference  
   * @param mag Magnet reference
   */
  RTLoop(ControlAPI& a, BPM& bpm, Magnet& mag): api(a), bpm_ref(bpm), magnet_ref(mag) {
    // Set up MPS beam abort callback
    mps.set_beam_abort_callback([this]() {
      emergency_stop.store(true);
      control_enabled.store(false);
      api.set_magnet(0.0);
    });
    
    // Set up MPS alarm callback  
    mps.set_alarm_callback([](const std::string& message) {
      std::cout << "MPS ALARM: " << message << std::endl;
    });
  }

  /**
   * @brief Main control loop execution
   * @param pub Telemetry publisher (template for flexibility)
   * @param rep Control responder (template for flexibility)
   */
  template<class Pub, class Rep>
  void run(Pub& pub, Rep& rep) {
    auto period_ns = std::chrono::nanoseconds((long long)(1e9/hz));
    PeriodicClock clk(period_ns);
    Watchdog wd(period_ns);
    auto t0 = std::chrono::steady_clock::now();
    
    while (running.load(std::memory_order_relaxed)) {
      auto start = std::chrono::steady_clock::now();

      // read sensors
      double pos = api.read_pos();
      double intensity = api.read_intensity();
      
      // Check Machine Protection System
      bool mps_safe = mps.check_safety(intensity, pos);
      if (!mps_safe) {
        // MPS triggered abort - force emergency stop
        emergency_stop.store(true);
        control_enabled.store(false);
      }

      // control (only if enabled and not in emergency stop)
      double u = 0.0;
      if (control_enabled.load() && !emergency_stop.load()) {
        double dt = std::chrono::duration<double>(period_ns).count();
        u = pid.step(pos, dt, lim.magnet_min, lim.magnet_max);
        api.set_magnet(u);
        bpm_ref.inject_offset(-0.4 * u); // magnet influences position
      } else {
        // Control disabled or emergency stop - hold magnet at zero
        api.set_magnet(0.0);
      }

      // watchdog
      auto end = std::chrono::steady_clock::now();
      bool deadline_missed = wd.check(start, end);
      if (deadline_missed) {
        deadline_misses.fetch_add(1);
      }
      
      loop_count.fetch_add(1);

      // publish telemetry
      double t = std::chrono::duration<double>(end - t0).count();
      json j = {{"t",t},{"pos",pos},{"intensity",intensity},
                {"mag",api.get_magnet()},{"deadline_miss", wd.is_tripped()?1:0},
                {"mps_safe", mps.is_beam_permitted()},
                {"mps_abort", mps.is_abort_active()}};
      pub.send(j.dump());

      // non-blocking control handling
      zmq_pollitem_t items[] = {{rep.rep, 0, ZMQ_POLLIN, 0}};
      zmq_poll(items, 1, 0);
      if (items[0].revents & ZMQ_POLLIN) {
        std::string cmd = rep.recv();
        auto r = handle_cmd(cmd, period_ns);
        rep.reply(r);
      }

      clk.wait_next();
      wd.reset();
    }
  }

  /**
   * @brief Handle incoming JSON commands
   * @param s JSON command string
   * @param period_ns Reference to current period (updated for frequency changes)
   * @return JSON response string
   */
  std::string handle_cmd(const std::string& s, std::chrono::nanoseconds& period_ns){
    auto j = json::parse(s, nullptr, false);
    if (!j.is_object()) return "{\"ok\":false}";
    
    if (j["cmd"] == "set_pid"){
      pid.kp = j.value("kp", pid.kp);
      pid.ki = j.value("ki", pid.ki);
      pid.kd = j.value("kd", pid.kd);
      return "{\"ok\":true}";
    } else if (j["cmd"] == "set_freq"){
      double new_hz = std::max(10.0, std::min(2000.0, j.value("hz", hz)));
      hz = new_hz;
      period_ns = std::chrono::nanoseconds((long long)(1e9/hz));
      return "{\"ok\":true}";
    } else if (j["cmd"] == "set_setpoint"){
      pid.setpoint = j.value("sp", 0.0);
      return "{\"ok\":true}";
    } else if (j["cmd"] == "recommission"){
      pid.integ = 0.0; 
      pid.prev_err = 0.0; 
      magnet_ref.set(0.0); 
      bpm_ref.inject_offset(0.0);
      emergency_stop.store(false);
      control_enabled.store(true);
      mps.reset_mps(); // Reset machine protection system
      return "{\"ok\":true}";
    } else if (j["cmd"] == "emergency_stop"){
      emergency_stop.store(true);
      control_enabled.store(false);
      api.set_magnet(0.0);
      return "{\"ok\":true}";
    } else if (j["cmd"] == "enable_control"){
      bool enable = j.value("enable", true);
      if (!emergency_stop.load()) {
        control_enabled.store(enable);
        if (!enable) {
          api.set_magnet(0.0);
        }
      }
      return "{\"ok\":true}";
    } else if (j["cmd"] == "get_status"){
      json status = {
        {"ok", true},
        {"loop_frequency", hz},
        {"loop_count", loop_count.load()},
        {"deadline_misses", deadline_misses.load()},
        {"control_enabled", control_enabled.load()},
        {"emergency_stop", emergency_stop.load()},
        {"mps_safe", mps.is_beam_permitted()},
        {"mps_abort_count", mps.get_abort_count()},
        {"pid_gains", {{"kp", pid.kp}, {"ki", pid.ki}, {"kd", pid.kd}}},
        {"setpoint", pid.setpoint}
      };
      return status.dump();
    }
    return "{\"ok\":false}";
  }
};