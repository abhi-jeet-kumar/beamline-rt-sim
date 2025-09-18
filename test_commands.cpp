#include "src/control/loop.hpp"
#include "src/hw/simple_bpm.hpp"
#include "src/hw/simple_bic.hpp"
#include "src/hw/simple_magnet.hpp"
#include <iostream>

int main() {
    BPM bpm; BIC bic; Magnet mag;
    ControlAPI api{bpm, bic, mag};
    RTLoop loop(api, bpm, mag);
    
    std::chrono::nanoseconds period_ns(1000000);
    
    // Test get_status
    std::string status = loop.handle_cmd(R"({"cmd":"get_status"})", period_ns);
    std::cout << "Status: " << status << std::endl;
    
    // Test emergency_stop
    std::string estop = loop.handle_cmd(R"({"cmd":"emergency_stop"})", period_ns);
    std::cout << "Emergency stop: " << estop << std::endl;
    
    // Test enable_control
    std::string enable = loop.handle_cmd(R"({"cmd":"enable_control","enable":false})", period_ns);
    std::cout << "Enable control: " << enable << std::endl;
    
    return 0;
}
