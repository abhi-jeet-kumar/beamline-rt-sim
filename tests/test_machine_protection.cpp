#include "../src/safety/machine_protection_system.hpp"
#include <cassert>
#include <iostream>
#include <thread>

/**
 * @brief Test Machine Protection System functionality
 * 
 * Tests BLM thresholds, automatic beam abort, and safety callbacks
 */
int main() {
    std::cout << "Testing Machine Protection System..." << std::endl;
    
    // Test 1: BLM basic functionality
    {
        std::cout << "Test 1: BLM basic functionality" << std::endl;
        
        BeamLossMonitor blm("TEST_BLM");
        
        // Test safe conditions
        bool safe = blm.update_measurement(100.0, 0.1); // Normal beam
        assert(safe);
        assert(!blm.is_warning_active());
        assert(!blm.is_abort_active());
        
        std::cout << "  BLM safe conditions test passed" << std::endl;
    }
    
    // Test 2: BLM threshold testing
    {
        std::cout << "Test 2: BLM threshold testing" << std::endl;
        
        BeamLossMonitor blm("THRESHOLD_BLM");
        blm.set_warning_threshold(1e-7);
        blm.set_abort_threshold(1e-6);
        
        bool warning_triggered = false;
        bool abort_triggered = false;
        
        blm.set_warning_callback([&](const std::string& id, double rate) {
            warning_triggered = true;
            std::cout << "    Warning triggered: " << id << " rate: " << rate << std::endl;
        });
        
        blm.set_abort_callback([&](const std::string& id, double rate) {
            abort_triggered = true;
            std::cout << "    Abort triggered: " << id << " rate: " << rate << std::endl;
        });
        
        // Test high loss rate condition
        bool safe = blm.update_measurement(10000.0, 5.0); // High current, off-center
        
        // Should trigger both warning and abort
        assert(warning_triggered);
        assert(abort_triggered);
        assert(!safe);
        assert(blm.is_abort_active());
        
        std::cout << "  BLM threshold testing passed" << std::endl;
    }
    
    // Test 3: MPS system integration
    {
        std::cout << "Test 3: MPS system integration" << std::endl;
        
        MachineProtectionSystem mps;
        
        bool abort_called = false;
        std::string alarm_message;
        
        mps.set_beam_abort_callback([&]() {
            abort_called = true;
            std::cout << "    Beam abort callback triggered" << std::endl;
        });
        
        mps.set_alarm_callback([&](const std::string& msg) {
            alarm_message = msg;
            std::cout << "    Alarm: " << msg << std::endl;
        });
        
        // Test safe conditions
        assert(mps.is_beam_permitted());
        bool safe = mps.check_safety(100.0, 0.1);
        assert(safe);
        
        // Test dangerous conditions (high current, very off-center)
        safe = mps.check_safety(50000.0, 10.0);
        
        // Should trigger abort
        assert(abort_called);
        assert(!mps.is_beam_permitted());
        assert(mps.is_abort_active());
        
        // Test reset
        mps.reset_mps();
        assert(mps.is_beam_permitted());
        assert(!mps.is_abort_active());
        
        std::cout << "  MPS system integration test passed" << std::endl;
    }
    
    // Test 4: Multiple BLM coordination
    {
        std::cout << "Test 4: Multiple BLM coordination" << std::endl;
        
        MachineProtectionSystem mps;
        
        // Get BLM references
        auto* blm_upstream = mps.get_blm("BLM_UPSTREAM");
        auto* blm_target = mps.get_blm("BLM_TARGET");
        auto* blm_downstream = mps.get_blm("BLM_DOWNSTREAM");
        
        assert(blm_upstream != nullptr);
        assert(blm_target != nullptr);
        assert(blm_downstream != nullptr);
        
        // Test that all BLMs are monitored
        bool safe = mps.check_safety(1000.0, 0.5);
        assert(safe); // Should be safe with reasonable conditions
        
        // Get statistics from all BLMs
        auto all_stats = mps.get_all_blm_stats();
        assert(all_stats.size() == 3); // Should have 3 BLMs
        
        for (const auto& stats : all_stats) {
            assert(stats.total_measurements > 0); // All should have measurements
        }
        
        std::cout << "  Multiple BLM coordination test passed" << std::endl;
    }
    
    std::cout << "\n✅ All Machine Protection System tests passed!" << std::endl;
    return 0;
}
