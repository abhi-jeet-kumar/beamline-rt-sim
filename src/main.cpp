#include "hw/simple_bpm.hpp"
#include "hw/simple_bic.hpp"
#include "hw/simple_magnet.hpp"
#include "control/api.hpp"
#include "control/loop.hpp"
#include "ipc/telemetry_pub.hpp"
#include "ipc/control_rep.hpp"

/**
 * @brief Main application entry point
 * 
 * Implements the exact structure specified in technical_details.txt:
 * - Component instantiation: BPM bpm; BIC bic; Magnet mag;
 * - ControlAPI creation: ControlAPI api{bpm, bic, mag};
 * - IPC setup: TelemetryPub pub; ControlRep rep;
 * - RTLoop creation and configuration
 * - PID gains: kp=0.6, ki=0.05, kd=0.0
 * - loop.run(pub, rep) call
 */
#include <iostream>

int main(){
  std::cout << "Beamline RT Simulator - Starting up..." << std::endl;
  
  BPM bpm; 
  BIC bic; 
  Magnet mag;
  
  std::cout << "Hardware components created" << std::endl;
  
  ControlAPI api{bpm, bic, mag};
  TelemetryPub pub; 
  ControlRep rep;
  
  std::cout << "Control API and IPC initialized" << std::endl;
  std::cout << "Telemetry: tcp://127.0.0.1:5556" << std::endl;
  std::cout << "Control: tcp://127.0.0.1:5555" << std::endl;
  
  RTLoop loop(api, bpm, mag);
  loop.pid.kp = 0.6; 
  loop.pid.ki = 0.05; 
  loop.pid.kd = 0.0;
  
  std::cout << "Starting control loop at 1000 Hz..." << std::endl;
  std::cout << "PID gains: Kp=" << loop.pid.kp << ", Ki=" << loop.pid.ki << ", Kd=" << loop.pid.kd << std::endl;
  std::cout << "Press Ctrl+C to stop" << std::endl;
  
  loop.run(pub, rep);
  return 0;
}
