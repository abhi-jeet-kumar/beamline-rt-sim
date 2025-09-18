#pragma once
#include "simple_iactuator.hpp"

/**
 * @brief Simple Magnet implementation per technical specification
 * 
 * Implements exactly the interface specified in technical_details.txt
 */
struct Magnet : IActuator {
  double current{0.0};
  
  // maps current to BPM offset via BPM::inject_offset from control layer
  void set(double v) override { current = v; }
  double get() const override { return current; }
};
