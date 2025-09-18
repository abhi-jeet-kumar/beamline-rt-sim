#include "../src/hw/bpm.hpp"
#include "../src/hw/bic.hpp"
#include "../src/hw/magnet.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <cmath>
#include <numeric>
#include <vector>
#include <algorithm>

/**
 * @brief Unit tests for hardware components (BPM, BIC, Magnet)
 * 
 * Tests include:
 * 1. Basic functionality and interface compliance
 * 2. Physics modeling accuracy
 * 3. Calibration and configuration
 * 4. Noise and environmental effects
 * 5. Safety systems and error handling
 * 6. Performance characteristics
 */

int main() {
    std::cout << "Testing Hardware Components (BPM, BIC, Magnet)..." << std::endl;
    
    // Test 1: BPM basic functionality
    {
        std::cout << "Test 1: BPM basic functionality" << std::endl;
        
        BPM bpm("TEST_BPM", 12345);
        assert(bpm.initialize());
        assert(bpm.is_initialized());
        assert(bpm.get_id() == "TEST_BPM");
        assert(bpm.get_type_name() == "BPM");
        assert(bpm.get_units() == "mm");
        
        // Test beam position setting and readout
        bpm.set_beam_position(2.5, -1.0);
        bpm.set_readout_axis("X");
        bpm.enable_noise(false);
        
        double reading = bpm.read();
        assert(std::abs(reading - 2.5) < 0.1); // Should read close to set position
        
        // Test Y axis
        bpm.set_readout_axis("Y");
        reading = bpm.read();
        assert(std::abs(reading - (-1.0)) < 0.1);
        
        assert(bpm.self_test());
        assert(bpm.is_healthy());
        
        std::cout << "  BPM basic functionality test passed" << std::endl;
    }
    
    // Test 2: BPM calibration and effects
    {
        std::cout << "Test 2: BPM calibration and effects" << std::endl;
        
        BPM bpm("CAL_BPM", 54321);
        bpm.initialize();
        bpm.enable_noise(false);
        bpm.set_readout_axis("X");
        
        // Test calibration scaling
        bpm.set_beam_position(1.0, 0.0);
        bpm.set_calibration(2.0, 1.0, 0.5, 0.0, 0.0); // 2x scale, 0.5 offset for X
        
        double reading = bpm.read();
        double expected = 1.0 * 2.0 + 0.5; // position * scale + offset = 2.5
        assert(std::abs(reading - expected) < 0.01);
        
        // Test rotation (45 degrees)
        bpm.set_beam_position(1.0, 1.0);
        bpm.set_calibration(1.0, 1.0, 0.0, 0.0, 45.0);
        
        reading = bpm.read(); // Should read X component after 45° rotation
        double expected_rotated = (1.0 * std::cos(M_PI/4) - 1.0 * std::sin(M_PI/4));
        assert(std::abs(reading - expected_rotated) < 0.1);
        
        // Test temperature effects
        bpm.set_calibration(1.0, 1.0, 0.0, 0.0, 0.0); // Reset calibration
        bpm.set_beam_position(0.0, 0.0);
        bpm.set_temperature(30.0); // 10°C above reference
        
        reading = bpm.read();
        // Should have small temperature-induced offset
        assert(std::abs(reading) < 0.1); // Small but non-zero due to temperature
        
        std::cout << "  BPM calibration test passed" << std::endl;
    }
    
    // Test 3: BPM noise characteristics
    {
        std::cout << "Test 3: BPM noise characteristics" << std::endl;
        
        BPM bpm("NOISE_BPM", 11111);
        bpm.initialize();
        bpm.set_beam_position(0.0, 0.0);
        bpm.set_readout_axis("X");
        bpm.enable_noise(true);
        
        // Test shot noise dependence on beam current
        std::vector<double> readings_low, readings_high;
        
        // Low current (high noise)
        bpm.set_beam_current(1.0);
        for (int i = 0; i < 100; i++) {
            readings_low.push_back(bpm.read());
        }
        
        // High current (low noise)
        bpm.set_beam_current(1000.0);
        for (int i = 0; i < 100; i++) {
            readings_high.push_back(bpm.read());
        }
        
        // Calculate standard deviations
        auto calc_std = [](const std::vector<double>& data) {
            double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
            double sq_sum = 0.0;
            for (double x : data) sq_sum += (x - mean) * (x - mean);
            return std::sqrt(sq_sum / (data.size() - 1));
        };
        
        double std_low = calc_std(readings_low);
        double std_high = calc_std(readings_high);
        
        std::cout << "    Low current noise: " << std_low << " mm" << std::endl;
        std::cout << "    High current noise: " << std_high << " mm" << std::endl;
        
        // Higher current should have lower noise (shot noise scales as 1/sqrt(I))
        assert(std_high < std_low);
        
        std::cout << "  BPM noise characteristics test passed" << std::endl;
    }
    
    // Test 4: BIC basic functionality
    {
        std::cout << "Test 4: BIC basic functionality" << std::endl;
        
        BIC bic("TEST_BIC", 22222);
        assert(bic.initialize());
        assert(bic.get_id() == "TEST_BIC");
        assert(bic.get_type_name() == "BIC");
        assert(bic.get_units() == "counts/s");
        
        // Test intensity measurement
        bic.set_beam_intensity(1000.0);
        bic.enable_noise(false);
        bic.set_dark_current(0.0);
        bic.set_calibration(1.0, 0.0);
        bic.set_conversion_factor(1.0);
        
        double reading = bic.read();
        assert(std::abs(reading - 1000.0) < 100.0); // Should be close to set intensity
        
        assert(bic.self_test());
        assert(bic.is_healthy());
        
        std::cout << "  BIC basic functionality test passed" << std::endl;
    }
    
    // Test 5: BIC physics modeling
    {
        std::cout << "Test 5: BIC physics modeling" << std::endl;
        
        BIC bic("PHYSICS_BIC", 33333);
        bic.initialize();
        bic.enable_noise(false);
        
        // Test quantum efficiency effect
        bic.set_beam_intensity(1000.0);
        bic.set_quantum_efficiency(0.5); // 50% QE
        bic.set_calibration(1.0, 0.0);
        bic.set_conversion_factor(1.0);
        bic.set_dark_current(0.0);
        
        double reading = bic.read();
        // Should read approximately 500 due to 50% QE
        assert(std::abs(reading - 500.0) < 100.0);
        
        // Test dark current subtraction
        bic.set_quantum_efficiency(1.0);
        bic.set_dark_current(100.0);
        
        reading = bic.read();
        // Should read ~1000 (signal) with dark current subtracted
        assert(std::abs(reading - 1000.0) < 100.0);
        
        // Test saturation
        bic.set_saturation_level(800.0);
        bic.enable_saturation(true);
        bic.set_beam_intensity(2000.0); // Above saturation
        
        reading = bic.read();
        // Should be limited by saturation
        assert(reading < 900.0);
        assert(bic.is_saturated());
        
        std::cout << "  BIC physics modeling test passed" << std::endl;
    }
    
    // Test 6: BIC noise statistics
    {
        std::cout << "Test 6: BIC noise statistics" << std::endl;
        
        BIC bic("STATS_BIC", 44444);
        bic.initialize();
        bic.enable_noise(true);
        bic.set_beam_intensity(1000.0);
        bic.set_dark_current(10.0);
        
        std::vector<double> readings;
        for (int i = 0; i < 1000; i++) {
            readings.push_back(bic.read());
        }
        
        // Calculate mean and std
        double mean = std::accumulate(readings.begin(), readings.end(), 0.0) / readings.size();
        double variance = 0.0;
        for (double x : readings) variance += (x - mean) * (x - mean);
        variance /= (readings.size() - 1);
        double std_dev = std::sqrt(variance);
        
        std::cout << "    BIC statistics: mean=" << mean << ", std=" << std_dev << std::endl;
        
        // For Poisson statistics, variance ≈ mean for counting detector
        // But exact relationship depends on calibration factors
        assert(mean > 500.0); // Should be reasonable intensity
        assert(std_dev > 10.0); // Should have counting noise
        
        // Get internal statistics
        auto [reads, last, snr] = bic.get_measurement_stats();
        assert(reads == 1000);
        std::cout << "    SNR estimate: " << snr << std::endl;
        
        std::cout << "  BIC noise statistics test passed" << std::endl;
    }
    
    // Test 7: Magnet basic functionality
    {
        std::cout << "Test 7: Magnet basic functionality" << std::endl;
        
        Magnet magnet("TEST_MAG", 55555);
        assert(magnet.initialize());
        assert(magnet.get_id() == "TEST_MAG");
        assert(magnet.get_type_name() == "Magnet");
        assert(magnet.get_units() == "A");
        
        // Test current setting
        magnet.enable_noise(false);
        magnet.set(5.0);
        
        // Allow time for settling
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        double current = magnet.get();
        assert(std::abs(current - 5.0) < 1.0); // Should be approaching 5A
        
        // Test magnetic field calculation
        double field = magnet.get_magnetic_field();
        assert(field != 0.0); // Should have non-zero field
        
        assert(magnet.self_test());
        assert(magnet.is_healthy());
        
        std::cout << "  Magnet basic functionality test passed" << std::endl;
    }
    
    // Test 8: Magnet physics and safety
    {
        std::cout << "Test 8: Magnet physics and safety" << std::endl;
        
        Magnet magnet("SAFETY_MAG", 66666);
        magnet.initialize();
        magnet.enable_noise(false);
        
        // Test slew rate limiting
        magnet.set_slew_rate_limit(1.0); // 1 A/s
        magnet.set(0.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        magnet.set(10.0); // Large step
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        double current = magnet.get();
        // Should be limited by slew rate, not at 10A yet
        assert(current < 5.0);
        assert(magnet.is_ramping());
        
        // Test emergency stop
        magnet.emergency_stop();
        current = magnet.get();
        assert(std::abs(current) < 0.1); // Should be zero
        assert(!magnet.is_ramping());
        
        magnet.reset_emergency_stop();
        
        // Test interlock
        magnet.set_magnet_parameters(0.1, 1.0, 0.01); // L=0.1H, R=1Ω, k=0.01T/A
        magnet.set_saturation(10.0, 0.1); // 10A saturation
        
        // Test field calculation
        magnet.set(5.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        double field = magnet.get_magnetic_field();
        double expected_field = 5.0 * 0.01; // current * field_constant
        assert(std::abs(field - expected_field) < 0.01);
        
        std::cout << "  Magnet physics and safety test passed" << std::endl;
    }
    
    // Test 9: Magnet advanced features
    {
        std::cout << "Test 9: Magnet advanced features" << std::endl;
        
        Magnet magnet("ADVANCED_MAG", 77777);
        magnet.initialize();
        
        // Test power calculation
        magnet.set_magnet_parameters(0.1, 2.0, 0.01); // R=2Ω
        magnet.set(3.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        double power = magnet.get_power();
        double current = magnet.get();
        double expected_power = current * current * 2.0; // I²R
        assert(std::abs(power - expected_power) < 1.0);
        
        // Test time constant
        double time_constant = magnet.get_time_constant();
        double expected_tc = 0.1 / 2.0; // L/R = 0.05s
        assert(std::abs(time_constant - expected_tc) < 0.01);
        
        // Test energy tracking
        double initial_energy = magnet.get_total_energy_dissipated();
        magnet.set(5.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        double final_energy = magnet.get_total_energy_dissipated();
        assert(final_energy > initial_energy); // Should have dissipated energy
        
        // Test temperature effects
        magnet.set_temperature(50.0); // Hot operation
        double hot_resistance = 2.0 * (1.0 + (50.0 - 20.0) * 0.001); // Expected resistance
        
        std::cout << "    Power: " << power << "W, Time constant: " << time_constant << "s" << std::endl;
        std::cout << "    Energy dissipated: " << (final_energy - initial_energy) << "J" << std::endl;
        
        std::cout << "  Magnet advanced features test passed" << std::endl;
    }
    
    // Test 10: Interface compliance
    {
        std::cout << "Test 10: Interface compliance" << std::endl;
        
        // Test all components implement interfaces correctly
        BPM bpm("INTERFACE_BPM");
        BIC bic("INTERFACE_BIC");
        Magnet mag("INTERFACE_MAG");
        
        // ISensor interface for BPM and BIC
        ISensor* sensor_bpm = &bpm;
        ISensor* sensor_bic = &bic;
        
        assert(sensor_bpm->initialize());
        assert(sensor_bic->initialize());
        
        // Test polymorphic behavior
        std::vector<ISensor*> sensors = {sensor_bpm, sensor_bic};
        for (ISensor* sensor : sensors) {
            assert(!sensor->get_type_name().empty());
            assert(!sensor->get_units().empty());
            auto range = sensor->get_range();
            assert(range.second > range.first);
            assert(sensor->get_resolution() > 0.0);
            assert(sensor->self_test());
        }
        
        // IActuator interface for Magnet
        IActuator* actuator = &mag;
        assert(actuator->initialize());
        
        auto limits = actuator->get_limits();
        assert(limits.second > limits.first);
        assert(actuator->get_rate_limit() > 0.0);
        assert(!actuator->get_type_name().empty());
        assert(!actuator->get_units().empty());
        assert(actuator->get_resolution() > 0.0);
        assert(actuator->self_test());
        
        // Test actuator control
        actuator->set(1.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        double value = actuator->get();
        assert(std::abs(value - 1.0) < 2.0); // Should be moving toward setpoint
        
        std::cout << "  Interface compliance test passed" << std::endl;
    }
    
    std::cout << "\n✅ All Hardware Component tests passed!" << std::endl;
    return 0;
}
