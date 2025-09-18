#pragma once
#include <random>

/**
 * @brief Simple noise generator per technical specification
 * 
 * Implements exactly the interface specified in technical_details.txt
 */
struct Noise {
  std::mt19937 gen{std::random_device{}()};
  std::normal_distribution<double> n{0.0, 0.01};
  
  double gauss() { 
    return n(gen); 
  }
  
  double poisson_mean(double mean) {
    std::poisson_distribution<int> p(mean);
    return static_cast<double>(p(gen));
  }
};
