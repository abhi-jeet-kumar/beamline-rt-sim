#pragma once

/**
 * @brief Simple actuator interface per technical specification
 * 
 * Implements exactly the interface specified in technical_details.txt
 */
struct IActuator { 
  virtual ~IActuator() = default; 
  virtual void set(double) = 0; 
  virtual double get() const = 0; 
};
