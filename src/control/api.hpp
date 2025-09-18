#pragma once
#include "../hw/simple_isensor.hpp"
#include "../hw/simple_iactuator.hpp"

/**
 * @brief Control API for hardware abstraction
 * 
 * Provides a unified interface to hardware components following
 * the FESA-style abstraction specified in technical_details.txt
 */
struct ControlAPI {
  ISensor& bpm;      ///< Beam Position Monitor reference
  ISensor& bic;      ///< Beam Intensity Counter reference  
  IActuator& magnet; ///< Magnet actuator reference
  
  /**
   * @brief Set magnet current
   * @param v Current value in Amperes
   */
  void set_magnet(double v) { magnet.set(v); }
  
  /**
   * @brief Get current magnet current
   * @return Current in Amperes
   */
  double get_magnet() const { return magnet.get(); }
  
  /**
   * @brief Read beam position
   * @return Position in mm
   */
  double read_pos() { return bpm.read(); }
  
  /**
   * @brief Read beam intensity
   * @return Intensity in detector units
   */
  double read_intensity() { return bic.read(); }
};