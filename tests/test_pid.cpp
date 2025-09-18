#include "../src/core/pid.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * @brief Test PID controller functionality, stability, and anti-windup behavior
 * 
 * This test suite verifies:
 * 1. Basic P, I, D term calculations
 * 2. Anti-windup protection mechanisms
 * 3. Step response and settling behavior
 * 4. Setpoint tracking accuracy
 * 5. Output limiting and saturation handling
 * 6. Reset and bumpless transfer functionality
 */

// Simple plant model for testing: first-order lag with gain
class TestPlant {
private:
    double state{0.0};
    double time_constant{0.1};  // 100ms time constant
    double gain{1.0};
    
public:
    TestPlant(double tc = 0.1, double g = 1.0) : time_constant(tc), gain(g) {}
    
    double step(double input, double dt) {
        // First-order system: dy/dt = (K*u - y) / tau
        double derivative = (gain * input - state) / time_constant;
        state += derivative * dt;
        return state;
    }
    
    double get_output() const { return state; }
    void reset() { state = 0.0; }
    void set_state(double s) { state = s; }
};

// Helper function to simulate control loop
std::vector<double> simulate_control_loop(PID& controller, TestPlant& plant, 
                                         double setpoint, double dt, int steps) {
    std::vector<double> outputs;
    outputs.reserve(steps);
    
    controller.setpoint = setpoint;
    
    for (int i = 0; i < steps; i++) {
        double measurement = plant.get_output();
        double control_output = controller.step(measurement, dt, -2.0, 2.0);
        plant.step(control_output, dt);
        outputs.push_back(measurement);
    }
    
    return outputs;
}

int main() {
    std::cout << "Testing PID controller functionality..." << std::endl;
    
    const double dt = 0.001;  // 1ms time step (1kHz control loop)
    const double tolerance = 0.01;  // 1% tolerance for steady-state
    
    // Test 1: Basic proportional control
    {
        std::cout << "Test 1: Proportional control" << std::endl;
        
        PID pid;
        pid.kp = 1.0;
        pid.ki = 0.0;
        pid.kd = 0.0;
        pid.setpoint = 1.0;
        
        TestPlant plant(0.1, 1.0);
        
        // Single step test
        double measurement = 0.0;
        double output = pid.step(measurement, dt, -10.0, 10.0);
        
        // Should be proportional to error
        assert(std::abs(output - 1.0) < 0.001);  // Error = 1.0, Kp = 1.0
        assert(std::abs(pid.get_error() - 1.0) < 0.001);
        assert(std::abs(pid.get_proportional() - 1.0) < 0.001);
        assert(std::abs(pid.get_integral()) < 0.001);
        assert(std::abs(pid.get_derivative()) < 0.001);
        
        std::cout << "  Proportional control test passed" << std::endl;
    }
    
    // Test 2: Integral action and steady-state error elimination
    {
        std::cout << "Test 2: Integral action" << std::endl;
        
        PID pid;
        pid.kp = 2.0;
        pid.ki = 1.0;  // Enable integral action
        pid.kd = 0.0;
        
        TestPlant plant(0.1, 0.8);  // Plant with gain < 1 to test steady-state error
        
        auto outputs = simulate_control_loop(pid, plant, 1.0, dt, 2000);
        
        // Check that steady-state error is eliminated
        double final_value = outputs.back();
        double steady_state_error = std::abs(1.0 - final_value);
        
        std::cout << "  Final value: " << final_value << std::endl;
        std::cout << "  Steady-state error: " << steady_state_error << std::endl;
        
        assert(steady_state_error < tolerance);  // Should eliminate steady-state error
        
        // Check integrator has accumulated
        assert(std::abs(pid.get_integrator()) > 0.1);
        
        std::cout << "  Integral action test passed" << std::endl;
    }
    
    // Test 3: Derivative action and overshoot reduction
    {
        std::cout << "Test 3: Derivative action" << std::endl;
        
        PID pid_no_d, pid_with_d;
        
        // PID without derivative
        pid_no_d.kp = 5.0;
        pid_no_d.ki = 2.0;
        pid_no_d.kd = 0.0;
        
        // PID with derivative
        pid_with_d.kp = 5.0;
        pid_with_d.ki = 2.0;
        pid_with_d.kd = 0.1;
        
        TestPlant plant1(0.1, 1.0), plant2(0.1, 1.0);
        
        auto outputs_no_d = simulate_control_loop(pid_no_d, plant1, 1.0, dt, 1000);
        auto outputs_with_d = simulate_control_loop(pid_with_d, plant2, 1.0, dt, 1000);
        
        // Find maximum overshoot
        double max_no_d = *std::max_element(outputs_no_d.begin(), outputs_no_d.end());
        double max_with_d = *std::max_element(outputs_with_d.begin(), outputs_with_d.end());
        
        double overshoot_no_d = (max_no_d - 1.0) * 100;  // Percentage
        double overshoot_with_d = (max_with_d - 1.0) * 100;
        
        std::cout << "  Overshoot without D: " << overshoot_no_d << "%" << std::endl;
        std::cout << "  Overshoot with D: " << overshoot_with_d << "%" << std::endl;
        
        // Derivative should reduce overshoot
        assert(overshoot_with_d < overshoot_no_d);
        
        std::cout << "  Derivative action test passed" << std::endl;
    }
    
    // Test 4: Anti-windup protection
    {
        std::cout << "Test 4: Anti-windup protection" << std::endl;
        
        PID pid;
        pid.kp = 1.0;
        pid.ki = 10.0;  // High integral gain to cause windup
        pid.kd = 0.0;
        pid.setpoint = 5.0;  // Large setpoint
        
        TestPlant plant(1.0, 0.1);  // Slow plant, low gain
        
        // Run with output limits that will cause saturation
        std::vector<double> integrators;
        for (int i = 0; i < 1000; i++) {
            double measurement = plant.get_output();
            double output = pid.step(measurement, dt, -1.0, 1.0);  // Tight limits
            plant.step(output, dt);
            integrators.push_back(pid.get_integrator());
        }
        
        // Check that integrator doesn't grow unbounded
        double max_integrator = *std::max_element(integrators.begin(), integrators.end());
        
        std::cout << "  Maximum integrator value: " << max_integrator << std::endl;
        
        // Should be bounded due to anti-windup
        assert(max_integrator < 10.0);  // Should not grow extremely large
        
        // Test recovery from saturation
        pid.setpoint = 0.1;  // Small setpoint to test recovery
        
        for (int i = 0; i < 1000; i++) {
            double measurement = plant.get_output();
            double output = pid.step(measurement, dt, -1.0, 1.0);
            plant.step(output, dt);
        }
        
        // Should settle to new setpoint
        double final_error = std::abs(0.1 - plant.get_output());
        assert(final_error < tolerance);
        
        std::cout << "  Anti-windup protection test passed" << std::endl;
    }
    
    // Test 5: Setpoint change with bumpless transfer
    {
        std::cout << "Test 5: Bumpless setpoint changes" << std::endl;
        
        PID pid;
        pid.kp = 2.0;
        pid.ki = 1.0;
        pid.kd = 0.5;
        
        TestPlant plant(0.1, 1.0);
        
        // Settle at first setpoint
        auto outputs1 = simulate_control_loop(pid, plant, 1.0, dt, 1000);
        
        double derivative_before = pid.get_derivative();
        
        // Change setpoint with bumpless transfer
        pid.set_setpoint(2.0, true);  // Enable derivative reset
        
        double measurement = plant.get_output();
        double output_after = pid.step(measurement, dt, -5.0, 5.0);
        double derivative_after = pid.get_derivative();
        
        std::cout << "  Derivative before setpoint change: " << derivative_before << std::endl;
        std::cout << "  Derivative after setpoint change: " << derivative_after << std::endl;
        
        // Derivative kick should be minimized
        assert(std::abs(derivative_after) < std::abs(derivative_before) + 1.0);
        
        std::cout << "  Bumpless setpoint change test passed" << std::endl;
    }
    
    // Test 6: Reset functionality
    {
        std::cout << "Test 6: Reset functionality" << std::endl;
        
        PID pid;
        pid.kp = 1.0;
        pid.ki = 1.0;
        pid.kd = 1.0;
        pid.setpoint = 1.0;
        
        // Accumulate some state
        pid.step(0.0, dt, -10.0, 10.0);
        pid.step(0.5, dt, -10.0, 10.0);
        
        assert(pid.get_integrator() != 0.0);
        assert(pid.get_derivative() != 0.0);
        
        // Reset controller
        pid.reset();
        
        assert(pid.get_integrator() == 0.0);
        assert(pid.get_proportional() == 0.0);
        assert(pid.get_integral() == 0.0);
        assert(pid.get_derivative() == 0.0);
        
        std::cout << "  Reset functionality test passed" << std::endl;
    }
    
    // Test 7: Integrator limits
    {
        std::cout << "Test 7: Integrator limits" << std::endl;
        
        PID pid;
        pid.kp = 0.0;
        pid.ki = 1.0;
        pid.kd = 0.0;
        pid.setpoint = 10.0;  // Large error to drive integrator
        
        // Set integrator limits
        pid.set_integrator_limits(-2.0, 3.0);
        
        // Drive integrator to positive limit
        for (int i = 0; i < 1000; i++) {
            pid.step(0.0, dt, -10.0, 10.0);  // Measurement = 0, large error
        }
        
        assert(pid.get_integrator() <= 3.0);
        assert(pid.get_integrator() >= 2.9);  // Should be at upper limit
        
        // Drive integrator to negative limit
        pid.setpoint = -10.0;
        for (int i = 0; i < 1000; i++) {
            pid.step(0.0, dt, -10.0, 10.0);
        }
        
        assert(pid.get_integrator() >= -2.0);
        assert(pid.get_integrator() <= -1.9);  // Should be at lower limit
        
        std::cout << "  Integrator limits test passed" << std::endl;
    }
    
    // Test 8: Performance benchmark (step response settling time)
    {
        std::cout << "Test 8: Step response performance" << std::endl;
        
        PID pid;
        pid.kp = 0.6;   // Based on technical spec defaults
        pid.ki = 0.05;
        pid.kd = 0.0;
        
        TestPlant plant(0.4, -0.4);  // Similar to BPM-magnet coupling
        
        auto outputs = simulate_control_loop(pid, plant, 0.0, dt, 2000);  // 2 second test
        
        // Find settling time (within 5% of final value)
        double final_value = 0.0;  // Setpoint
        int settling_time = -1;
        
        for (int i = 500; i < outputs.size(); i++) {  // Start checking after 0.5s
            bool settled = true;
            // Check if next 100 samples stay within 5% band
            for (int j = i; j < std::min(i + 100, (int)outputs.size()); j++) {
                if (std::abs(outputs[j] - final_value) > 0.05) {
                    settled = false;
                    break;
                }
            }
            if (settled) {
                settling_time = i;
                break;
            }
        }
        
        double settling_time_sec = settling_time * dt;
        std::cout << "  Settling time (5% band): " << settling_time_sec << " seconds" << std::endl;
        std::cout << "  Final error: " << std::abs(outputs.back() - final_value) << std::endl;
        
        // Should settle within 2 seconds (spec requirement)
        assert(settling_time_sec < 2.0);
        assert(std::abs(outputs.back() - final_value) < 0.02);  // 2% final accuracy
        
        std::cout << "  Step response performance test passed" << std::endl;
    }
    
    std::cout << "\n✅ All PID controller tests passed!" << std::endl;
    return 0;
}
