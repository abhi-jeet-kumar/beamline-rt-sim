#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <iomanip>

#include "hw/bpm.hpp"
#include "hw/bic.hpp"
#include "hw/magnet.hpp"
#include "control/api.hpp"
#include "control/loop.hpp"
#include "ipc/telemetry_pub.hpp"
#include "ipc/control_rep.hpp"

// Global flag for clean shutdown
std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
    std::cout << "\nShutdown signal received (" << signal << "), stopping..." << std::endl;
    shutdown_requested.store(true);
}

int main() {
    std::cout << "Beamline RT Simulator - Starting up..." << std::endl;
    
    // Install signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        // Initialize hardware simulation
        std::cout << "Initializing hardware simulation..." << std::endl;
        
        BPM bpm("BPM_01");
        BIC bic("BIC_01"); 
        Magnet magnet("MAG_01");
        
        // Initialize all components
        if (!bpm.initialize()) {
            std::cerr << "Failed to initialize BPM" << std::endl;
            return 1;
        }
        
        if (!bic.initialize()) {
            std::cerr << "Failed to initialize BIC" << std::endl;
            return 1;
        }
        
        if (!magnet.initialize()) {
            std::cerr << "Failed to initialize magnet" << std::endl;
            return 1;
        }
        
        // Set up initial beam conditions
        bpm.set_beam_position(0.5, 0.0);  // Start with 0.5mm offset
        bpm.set_beam_current(100.0);      // 100 mA beam current
        bic.set_beam_intensity(10000.0);  // Set BIC intensity
        
        // Create control API
        std::cout << "Setting up control system..." << std::endl;
        ControlAPI api(bpm, bic, magnet);
        
        // Verify system health
        if (!api.system_health_check()) {
            std::cerr << "System health check failed" << std::endl;
            std::cerr << api.get_system_status() << std::endl;
            return 1;
        }
        
        std::cout << api.get_system_status() << std::endl;
        
        // Initialize IPC
        std::cout << "Setting up IPC..." << std::endl;
        TelemetryPub telemetry_pub("tcp://127.0.0.1:5556");
        ControlRep control_rep("tcp://127.0.0.1:5555");
        
        if (!telemetry_pub.is_connected()) {
            std::cerr << "Failed to bind telemetry publisher" << std::endl;
            return 1;
        }
        
        if (!control_rep.is_connected()) {
            std::cerr << "Failed to bind control responder" << std::endl;
            return 1;
        }
        
        std::cout << "Telemetry publisher bound to: " << telemetry_pub.get_bind_address() << std::endl;
        std::cout << "Control responder bound to: " << control_rep.get_bind_address() << std::endl;
        
        // Create and configure control loop
        std::cout << "Starting control loop..." << std::endl;
        RTLoop control_loop(api, bpm, magnet);
        
        // Set initial PID parameters (conservative for stability)
        control_loop.handle_command(R"({"cmd":"set_pid","kp":0.6,"ki":0.05,"kd":0.0})");
        
        std::cout << "System ready! Control loop running at 1000 Hz" << std::endl;
        std::cout << "Connect GUI to:" << std::endl;
        std::cout << "  Telemetry: " << telemetry_pub.get_bind_address() << std::endl;
        std::cout << "  Control:   " << control_rep.get_bind_address() << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Run the control loop in a separate thread and monitor for shutdown
        std::thread loop_thread([&]() {
            try {
                control_loop.run(telemetry_pub, control_rep);
            } catch (const std::exception& e) {
                std::cerr << "Control loop error: " << e.what() << std::endl;
                shutdown_requested.store(true);
            }
        });
        
        // Monitor for shutdown request
        while (!shutdown_requested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Print periodic statistics
            static auto last_stats_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 10) {
                auto stats = control_loop.get_stats();
                std::cout << "Loop stats: " << stats.loop_count << " cycles, "
                         << stats.deadline_misses << " misses, "
                         << std::fixed << std::setprecision(3) 
                         << stats.avg_loop_time_ms << " ms avg, "
                         << stats.max_loop_time_ms << " ms max" << std::endl;
                last_stats_time = now;
            }
        }
        
        // Clean shutdown
        std::cout << "Stopping control loop..." << std::endl;
        control_loop.stop();
        
        if (loop_thread.joinable()) {
            loop_thread.join();
        }
        
        // Final statistics
        auto final_stats = control_loop.get_stats();
        std::cout << "Final statistics:" << std::endl;
        std::cout << "  Total cycles: " << final_stats.loop_count << std::endl;
        std::cout << "  Deadline misses: " << final_stats.deadline_misses << std::endl;
        std::cout << "  Average loop time: " << std::fixed << std::setprecision(3) 
                 << final_stats.avg_loop_time_ms << " ms" << std::endl;
        std::cout << "  Maximum loop time: " << final_stats.max_loop_time_ms << " ms" << std::endl;
        
        std::cout << "Shutdown complete." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
