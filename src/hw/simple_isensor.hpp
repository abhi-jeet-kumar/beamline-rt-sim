#pragma once

/**
 * @brief Simple sensor interface per technical specification
 * 
 * Implements exactly the interface specified in technical_details.txt
 */
struct ISensor { 
  virtual ~ISensor() = default; 
  virtual double read() = 0; 
};
