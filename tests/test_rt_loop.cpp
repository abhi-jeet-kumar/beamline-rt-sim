#include "../src/control/loop.hpp"
#include "../src/hw/simple_bpm.hpp"
#include "../src/hw/simple_bic.hpp"
#include "../src/hw/simple_magnet.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief Test RTLoop functionality
 * 
 * Tests the real-time control loop implementation including:
 * - Construction and configuration
 * - JSON command handling 
 * - PID parameter updates
 * - Frequency changes
 * - Recommission functionality
 */

// Mock publisher for testing
struct MockPublisher {
  int message_count = 0;
  std::string last_message;
  
  void send(const std::string& msg) {
    message_count++;
    last_message = msg;
  }
};

// Mock responder for testing  
struct MockResponder {
  std::string response_to_send = "";
  std::string last_command = "";
  bool has_command = false;
  
  struct {
    void* rep = nullptr;
  };
  
  bool has_request() { return has_command; }
  
  std::string recv() {
    has_command = false;
    return last_command;
  }
  
  void reply(const std::string& response) {
    response_to_send = response;
  }
  
  // Simulate incoming command
  void simulate_command(const std::string& cmd) {
    last_command = cmd;
    has_command = true;
  }
};

int main() {
    std::cout << "Testing RTLoop functionality..." << std::endl;
    
    // Create hardware components
    BPM bpm;
    BIC bic;
    Magnet mag;
    
    // Create control API
    ControlAPI api{bpm, bic, mag};
    
    // Create RTLoop
    RTLoop loop(api, bpm, mag);
    
    // Test 1: Construction and initial state
    {
        std::cout << "Test 1: RTLoop construction" << std::endl;
        
        assert(loop.hz == 1000.0);  // Default frequency
        assert(loop.running.load() == true);
        assert(loop.pid.kp == 0.1); // Default PID gains
        
        std::cout << "  RTLoop construction test passed" << std::endl;
    }
    
    // Test 2: JSON command handling
    {
        std::cout << "Test 2: JSON command handling" << std::endl;
        
        std::chrono::nanoseconds period_ns(1000000); // 1ms
        
        // Test set_pid command
        std::string pid_cmd = R"({"cmd":"set_pid","kp":2.0,"ki":1.0,"kd":0.5})";
        std::string response = loop.handle_cmd(pid_cmd, period_ns);
        
        assert(response == R"({"ok":true})");
        assert(loop.pid.kp == 2.0);
        assert(loop.pid.ki == 1.0);
        assert(loop.pid.kd == 0.5);
        
        // Test set_freq command
        std::string freq_cmd = R"({"cmd":"set_freq","hz":500})";
        response = loop.handle_cmd(freq_cmd, period_ns);
        
        assert(response == R"({"ok":true})");
        assert(loop.hz == 500.0);
        assert(period_ns.count() == 2000000); // 2ms period for 500Hz
        
        // Test set_setpoint command
        std::string sp_cmd = R"({"cmd":"set_setpoint","sp":1.5})";
        response = loop.handle_cmd(sp_cmd, period_ns);
        
        assert(response == R"({"ok":true})");
        assert(loop.pid.setpoint == 1.5);
        
        // Test recommission command
        loop.pid.integ = 5.0; // Set some integrator value
        std::string recomm_cmd = R"({"cmd":"recommission"})";
        response = loop.handle_cmd(recomm_cmd, period_ns);
        
        assert(response == R"({"ok":true})");
        assert(loop.pid.integ == 0.0);
        assert(loop.pid.prev_err == 0.0);
        
        // Test invalid command
        std::string invalid_cmd = R"({"cmd":"invalid"})";
        response = loop.handle_cmd(invalid_cmd, period_ns);
        assert(response == R"({"ok":false})");
        
        // Test malformed JSON
        std::string bad_json = "not json";
        response = loop.handle_cmd(bad_json, period_ns);
        assert(response == R"({"ok":false})");
        
        std::cout << "  JSON command handling test passed" << std::endl;
    }
    
    // Test 3: Frequency limits
    {
        std::cout << "Test 3: Frequency limits" << std::endl;
        
        std::chrono::nanoseconds period_ns(1000000);
        
        // Test frequency too low
        std::string low_freq = R"({"cmd":"set_freq","hz":5})";
        loop.handle_cmd(low_freq, period_ns);
        assert(loop.hz == 10.0); // Should be clamped to minimum
        
        // Test frequency too high  
        std::string high_freq = R"({"cmd":"set_freq","hz":5000})";
        loop.handle_cmd(high_freq, period_ns);
        assert(loop.hz == 2000.0); // Should be clamped to maximum
        
        // Test valid frequency
        std::string valid_freq = R"({"cmd":"set_freq","hz":1000})";
        loop.handle_cmd(valid_freq, period_ns);
        assert(loop.hz == 1000.0);
        
        std::cout << "  Frequency limits test passed" << std::endl;
    }
    
    std::cout << "\n✅ All RTLoop tests passed!" << std::endl;
    return 0;
}
