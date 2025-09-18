#include "../src/core/pid.hpp"
#include "stress_test_framework.hpp"
#include <cassert>
#include <iostream>
#include <random>

/**
 * @brief Comprehensive stress testing for PID Controller
 * 
 * Tests include:
 * 1. High-frequency control loop stress (1kHz sustained)
 * 2. Stability under parameter changes and disturbances
 * 3. Anti-windup behavior under extreme conditions
 * 4. Numerical stability with floating-point precision
 * 5. Performance under system load
 * 6. Long-term stability and drift analysis
 */

// Enhanced plant model for realistic testing
class StressTestPlant {
private:
    double state{0.0};
    double velocity{0.0};
    double noise_level{0.01};
    std::mt19937 rng{std::random_device{}()};
    std::normal_distribution<double> noise_dist{0.0, 1.0};
    
public:
    void set_noise_level(double level) { noise_level = level; }
    
    double step(double input, double dt) {
        // Second-order system with damping and noise
        double damping = 0.1;
        double natural_freq = 5.0; // rad/s
        
        // Add measurement noise
        double noise = noise_dist(rng) * noise_level;
        
        // Dynamics: d²x/dt² + 2*ζ*ωn*dx/dt + ωn²*x = ωn²*u
        double acceleration = natural_freq * natural_freq * input - 
                             2 * damping * natural_freq * velocity - 
                             natural_freq * natural_freq * state;
        
        velocity += acceleration * dt;
        state += velocity * dt;
        
        return state + noise;
    }
    
    double get_state() const { return state; }
    void reset(double initial_state = 0.0) { 
        state = initial_state; 
        velocity = 0.0; 
    }
    
    void add_disturbance(double magnitude) {
        state += magnitude;
    }
};

int main() {
    std::cout << "🔥 COMPREHENSIVE STRESS TESTING: PID Controller" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    // Test 1: High-frequency control loop stress
    {
        std::cout << "\n🚀 Test 1: High-frequency control loop (1kHz)" << std::endl;
        
        PID controller;
        controller.kp = 2.0;
        controller.ki = 1.0;
        controller.kd = 0.1;
        controller.setpoint = 1.0;
        
        StressTestPlant plant;
        StressTest::PerformanceMonitor monitor;
        
        const double dt = 0.001; // 1kHz
        const int iterations = 10000; // 10 seconds
        
        std::vector<double> errors;
        errors.reserve(iterations);
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            double measurement = plant.step(0.0, dt); // Get current state
            double control_output = controller.step(measurement, dt, -5.0, 5.0);
            plant.step(control_output, dt); // Apply control
            
            auto end = std::chrono::steady_clock::now();
            double exec_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(exec_time);
            
            errors.push_back(std::abs(controller.get_error()));
            
            // Check for reasonable execution time
            if (exec_time > 100.0) { // >100μs for single step
                monitor.record_deadline_miss();
            }
        }
        
        monitor.print_statistics("1kHz Control Loop");
        
        // Calculate settling performance
        double final_error = std::accumulate(errors.end() - 1000, errors.end(), 0.0) / 1000.0;
        std::cout << "  Final average error: " << final_error << std::endl;
        std::cout << "  Controller integrator: " << controller.get_integrator() << std::endl;
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 500000); // >500k control steps/sec
        assert(stats.p99_us < 50.0); // P99 < 50μs per control step
        assert(final_error < 0.1); // Good steady-state performance
        
        std::cout << "✅ High-frequency control stress test PASSED" << std::endl;
    }
    
    // Test 2: Stability under CPU stress
    {
        std::cout << "\n🚀 Test 2: Control stability under CPU stress" << std::endl;
        
        PID controller;
        controller.kp = 1.5;
        controller.ki = 0.5;
        controller.kd = 0.05;
        controller.setpoint = 0.0;
        
        StressTestPlant plant;
        StressTest::CPUStressor cpu_stress;
        
        cpu_stress.start_stress();
        
        const double dt = 0.001;
        std::vector<double> position_history;
        
        // Settle first
        for (int i = 0; i < 2000; i++) {
            double measurement = plant.get_state();
            double output = controller.step(measurement, dt, -3.0, 3.0);
            plant.step(output, dt);
        }
        
        // Apply step disturbance and measure recovery
        plant.add_disturbance(1.0); // 1 unit step disturbance
        
        for (int i = 0; i < 3000; i++) { // 3 seconds recovery
            double measurement = plant.get_state();
            double output = controller.step(measurement, dt, -3.0, 3.0);
            plant.step(output, dt);
            position_history.push_back(measurement);
        }
        
        cpu_stress.stop_stress();
        
        // Analyze recovery performance
        double final_position = std::accumulate(position_history.end() - 500, position_history.end(), 0.0) / 500.0;
        
        // Find settling time (within 5% of final value)
        int settling_time = -1;
        for (size_t i = 500; i < position_history.size() - 100; i++) {
            bool settled = true;
            for (size_t j = i; j < i + 100 && j < position_history.size(); j++) {
                if (std::abs(position_history[j] - final_position) > 0.05) {
                    settled = false;
                    break;
                }
            }
            if (settled) {
                settling_time = static_cast<int>(i);
                break;
            }
        }
        
        std::cout << "  Final position: " << final_position << std::endl;
        std::cout << "  Settling time: " << (settling_time * dt) << " seconds" << std::endl;
        
        assert(std::abs(final_position) < 0.1); // Good final tracking
        assert(settling_time > 0 && settling_time * dt < 2.0); // Settle within 2s
        
        std::cout << "✅ CPU stress stability test PASSED" << std::endl;
    }
    
    // Test 3: Anti-windup stress test
    {
        std::cout << "\n🚀 Test 3: Anti-windup under extreme conditions" << std::endl;
        
        PID controller;
        controller.kp = 5.0;
        controller.ki = 20.0; // Very high integral gain
        controller.kd = 0.0;
        controller.setpoint = 10.0; // Large setpoint
        
        StressTestPlant plant;
        plant.reset(0.0);
        
        const double dt = 0.001;
        const double output_limit = 1.0; // Very restrictive
        
        std::vector<double> integrator_values;
        std::vector<double> outputs;
        
        // Run with saturation for extended period
        for (int i = 0; i < 5000; i++) { // 5 seconds
            double measurement = plant.get_state();
            double output = controller.step(measurement, dt, -output_limit, output_limit);
            plant.step(output, dt);
            
            integrator_values.push_back(controller.get_integrator());
            outputs.push_back(output);
        }
        
        // Change to achievable setpoint
        controller.setpoint = 0.5;
        
        // Measure recovery time
        std::vector<double> recovery_errors;
        for (int i = 0; i < 3000; i++) { // 3 seconds recovery
            double measurement = plant.get_state();
            double output = controller.step(measurement, dt, -output_limit, output_limit);
            plant.step(output, dt);
            
            recovery_errors.push_back(std::abs(controller.get_error()));
        }
        
        // Analysis
        double max_integrator = *std::max_element(integrator_values.begin(), integrator_values.end());
        double final_error = std::accumulate(recovery_errors.end() - 500, recovery_errors.end(), 0.0) / 500.0;
        
        std::cout << "  Maximum integrator value: " << max_integrator << std::endl;
        std::cout << "  Final recovery error: " << final_error << std::endl;
        
        // Anti-windup should prevent unbounded integrator growth
        assert(max_integrator < 50.0); // Bounded integrator
        assert(final_error < 0.2); // Good recovery
        
        std::cout << "✅ Anti-windup stress test PASSED" << std::endl;
    }
    
    // Test 4: Numerical stability with extreme parameters
    {
        std::cout << "\n🚀 Test 4: Numerical stability test" << std::endl;
        
        PID controller;
        StressTest::PerformanceMonitor monitor;
        
        // Test with various extreme parameter combinations
        std::vector<std::tuple<double, double, double>> test_params = {
            {1000.0, 0.001, 100.0},  // High P, low I, high D
            {0.001, 1000.0, 0.001}, // Low P, high I, low D
            {100.0, 100.0, 100.0},  // All high
            {0.0001, 0.0001, 0.0001}, // All very low
        };
        
        for (auto [kp, ki, kd] : test_params) {
            controller.reset();
            controller.kp = kp;
            controller.ki = ki;
            controller.kd = kd;
            controller.setpoint = 1.0;
            
            bool numerically_stable = true;
            
            for (int i = 0; i < 1000; i++) {
                auto start = std::chrono::steady_clock::now();
                
                double measurement = std::sin(i * 0.001); // Varying input
                double output = controller.step(measurement, 0.001, -1000.0, 1000.0);
                
                auto end = std::chrono::steady_clock::now();
                double exec_time = std::chrono::duration<double, std::micro>(end - start).count();
                monitor.record_timing(exec_time);
                
                // Check for numerical issues
                if (!std::isfinite(output) || std::abs(output) > 1e6) {
                    numerically_stable = false;
                    break;
                }
            }
            
            std::cout << "  Params [P:" << kp << ", I:" << ki << ", D:" << kd << "] - " 
                      << (numerically_stable ? "STABLE" : "UNSTABLE") << std::endl;
            assert(numerically_stable);
        }
        
        monitor.print_statistics("Extreme Parameters Test");
        
        std::cout << "✅ Numerical stability test PASSED" << std::endl;
    }
    
    // Test 5: Rapid setpoint changes stress
    {
        std::cout << "\n🚀 Test 5: Rapid setpoint changes stress" << std::endl;
        
        PID controller;
        controller.kp = 2.0;
        controller.ki = 1.0;
        controller.kd = 0.1;
        
        StressTestPlant plant;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> setpoint_dist(-2.0, 2.0);
        
        const double dt = 0.001;
        std::vector<double> tracking_errors;
        
        for (int i = 0; i < 10000; i++) {
            // Change setpoint every 100 steps (every 0.1s)
            if (i % 100 == 0) {
                double new_setpoint = setpoint_dist(gen);
                controller.set_setpoint(new_setpoint, true); // With bumpless transfer
            }
            
            double measurement = plant.get_state();
            double output = controller.step(measurement, dt, -5.0, 5.0);
            plant.step(output, dt);
            
            tracking_errors.push_back(std::abs(controller.get_error()));
        }
        
        // Analyze tracking performance
        double mean_error = std::accumulate(tracking_errors.begin(), tracking_errors.end(), 0.0) / tracking_errors.size();
        double max_error = *std::max_element(tracking_errors.begin(), tracking_errors.end());
        
        std::cout << "  Mean tracking error: " << mean_error << std::endl;
        std::cout << "  Maximum tracking error: " << max_error << std::endl;
        
        assert(mean_error < 0.5); // Reasonable mean tracking
        assert(max_error < 3.0);  // Bounded maximum error
        
        std::cout << "✅ Rapid setpoint changes test PASSED" << std::endl;
    }
    
    // Test 6: Long-term stability and drift
    {
        std::cout << "\n🚀 Test 6: Long-term stability test" << std::endl;
        
        PID controller;
        controller.kp = 1.0;
        controller.ki = 0.1;
        controller.kd = 0.01;
        controller.setpoint = 0.0;
        
        StressTestPlant plant;
        plant.set_noise_level(0.05); // Add measurement noise
        
        const double dt = 0.001;
        const int long_duration = 60000; // 60 seconds
        
        std::vector<double> position_samples;
        std::vector<double> integrator_samples;
        
        // Sample every 100 steps for analysis
        for (int i = 0; i < long_duration; i++) {
            double measurement = plant.get_state();
            double output = controller.step(measurement, dt, -2.0, 2.0);
            plant.step(output, dt);
            
            if (i % 100 == 0) { // Sample every 0.1s
                position_samples.push_back(measurement);
                integrator_samples.push_back(controller.get_integrator());
            }
            
            // Add occasional disturbances
            if (i % 10000 == 0) {
                plant.add_disturbance(0.1);
            }
        }
        
        // Analyze long-term behavior
        double position_mean = std::accumulate(position_samples.begin(), position_samples.end(), 0.0) / position_samples.size();
        double position_variance = 0.0;
        for (double pos : position_samples) {
            position_variance += std::pow(pos - position_mean, 2);
        }
        position_variance /= position_samples.size();
        double position_std = std::sqrt(position_variance);
        
        double integrator_drift = std::abs(integrator_samples.back() - integrator_samples[100]); // Skip initial settling
        
        std::cout << "  Position mean: " << position_mean << std::endl;
        std::cout << "  Position std dev: " << position_std << std::endl;
        std::cout << "  Integrator drift: " << integrator_drift << std::endl;
        
        assert(std::abs(position_mean) < 0.1); // Low bias
        assert(position_std < 0.2); // Low variance
        assert(integrator_drift < 1.0); // Bounded integrator drift
        
        std::cout << "✅ Long-term stability test PASSED" << std::endl;
    }
    
    std::cout << "\n🎉 ALL PID STRESS TESTS PASSED!" << std::endl;
    std::cout << "📊 PID Controller validated for production control systems" << std::endl;
    
    return 0;
}
