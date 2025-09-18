#include "../src/control/api.hpp"
#include "../src/hw/bpm.hpp"
#include "../src/hw/bic.hpp"
#include "../src/hw/magnet.hpp"
#include <cassert>
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>

/**
 * @brief Test ControlAPI functionality
 * 
 * Verifies that the ControlAPI correctly wraps hardware interfaces
 * and provides unified access to BPM, BIC, and Magnet components
 */
int main() {
    std::cout << "Testing ControlAPI functionality..." << std::endl;
    
    // Create hardware components
    BPM bpm("API_TEST_BPM");
    BIC bic("API_TEST_BIC");
    Magnet magnet("API_TEST_MAG");
    
    // Initialize all components
    assert(bpm.initialize());
    assert(bic.initialize());
    assert(magnet.initialize());
    
    // Create ControlAPI with references
    ControlAPI api{bpm, bic, magnet};
    
    // Test BPM interface wrapping
    std::cout << "Testing BPM interface..." << std::endl;
    bpm.set_beam_position(1.5, 0.0);
    bpm.set_readout_axis("X");
    bpm.enable_noise(false);
    
    double pos = api.read_pos();
    assert(std::abs(pos - 1.5) < 0.5); // Should read close to set position
    
    // Test BIC interface wrapping
    std::cout << "Testing BIC interface..." << std::endl;
    bic.set_beam_intensity(5000.0);
    bic.enable_noise(false);
    bic.set_calibration(1.0, 0.0);
    bic.set_conversion_factor(1.0);
    
    double intensity = api.read_intensity();
    assert(intensity > 1000.0); // Should read reasonable intensity
    
    // Test Magnet interface wrapping
    std::cout << "Testing Magnet interface..." << std::endl;
    magnet.enable_noise(false);
    magnet.set_magnet_parameters(0.001, 1.0, 0.01); // Fast response
    magnet.set_saturation(100.0, 1.0);
    magnet.set_slew_rate_limit(100.0);
    
    double initial_current = api.get_magnet();
    api.set_magnet(2.0);
    
    // Allow magnet to respond (multiple calls needed for time evolution)
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        api.set_magnet(2.0);
    }
    
    double final_current = api.get_magnet();
    std::cout << "  Magnet current: " << final_current << "A (target: 2.0A)" << std::endl;
    
    // Verify magnet responded to commands
    assert(std::abs(final_current - initial_current) > 0.1);
    
    // Test that API preserves component identity
    assert(&api.bpm == &bpm);
    assert(&api.bic == &bic);
    assert(&api.magnet == &magnet);
    
    std::cout << "✅ All ControlAPI tests passed!" << std::endl;
    return 0;
}
