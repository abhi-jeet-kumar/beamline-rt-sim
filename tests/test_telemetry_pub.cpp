#include "../src/ipc/telemetry_pub.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

/**
 * @brief Test TelemetryPub functionality
 * 
 * Tests basic publisher creation, message sending, and cleanup
 * Note: This is a unit test that verifies the publisher can be created
 * and send messages without errors. Full integration testing requires
 * a separate subscriber.
 */
int main() {
    std::cout << "Testing TelemetryPub functionality..." << std::endl;
    
    try {
        // Test publisher creation and destruction
        {
            TelemetryPub pub;
            
            // Give ZeroMQ time to bind
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Test sending some sample telemetry messages
            std::string msg1 = R"({"t": 1.234, "pos": 0.5, "intensity": 1000.0, "mag": 1.5, "deadline_miss": 0})";
            std::string msg2 = R"({"t": 1.235, "pos": 0.6, "intensity": 1100.0, "mag": 1.6, "deadline_miss": 0})";
            std::string msg3 = R"({"t": 1.236, "pos": 0.7, "intensity": 1200.0, "mag": 1.7, "deadline_miss": 1})";
            
            pub.send(msg1);
            pub.send(msg2);
            pub.send(msg3);
            
            std::cout << "  Successfully sent 3 telemetry messages" << std::endl;
            
            // Allow time for messages to be sent
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } // TelemetryPub destructor called here
        
        std::cout << "  Publisher cleaned up successfully" << std::endl;
        
        // Test multiple publishers (to check resource cleanup)
        for (int i = 0; i < 3; i++) {
            TelemetryPub pub;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            std::string msg = R"({"t": )" + std::to_string(i) + R"(, "pos": 0.0, "intensity": 500.0, "mag": 0.0, "deadline_miss": 0})";
            pub.send(msg);
        }
        
        std::cout << "  Multiple publisher creation/cleanup test passed" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
    
    std::cout << "✅ All TelemetryPub tests passed!" << std::endl;
    return 0;
}
