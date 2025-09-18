#pragma once
#include "simple_isensor.hpp"
#include "simple_noise.hpp"
#include <cmath>

/**
 * @brief Simple BPM implementation per technical specification
 * 
 * Implements exactly the interface specified in technical_details.txt
 */
struct BPM : ISensor {
  Noise noise;
  double phase{0.0};          // rad
  double omega{2*M_PI*5.0};   // 5 Hz oscillation
  double offset{0.0};         // affected by magnet
  double step_dt{0.001};      // 1 kHz

  void inject_offset(double o){ offset = o; }

  double read() override {
    phase += omega * step_dt;
    return std::sin(phase) * 0.5 + offset + noise.gauss();
  }
};
