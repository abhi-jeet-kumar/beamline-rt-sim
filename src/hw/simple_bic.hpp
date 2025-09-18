#pragma once
#include "simple_isensor.hpp"
#include "simple_noise.hpp"

/**
 * @brief Simple BIC implementation per technical specification
 * 
 * Implements exactly the interface specified in technical_details.txt
 */
struct BIC : ISensor {
  Noise noise;
  double mean{10000.0};
  
  double read() override { 
    return noise.poisson_mean(mean); 
  }
};
