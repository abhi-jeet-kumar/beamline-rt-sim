#pragma once

/**
 * @brief Safety limits for beamline control system
 * 
 * Implements clamping logic to ensure all actuator commands
 * stay within safe operational bounds as specified in technical_details.txt
 */
struct Limits {
  double magnet_min{-2.0};  ///< Minimum magnet current in Amperes
  double magnet_max{ 2.0};  ///< Maximum magnet current in Amperes
  
  /**
   * @brief Clamp value to magnet current limits
   * @param v Input value to clamp
   * @return Clamped value within [magnet_min, magnet_max]
   */
  double clamp(double v) const {
    if (v < magnet_min) return magnet_min;
    if (v > magnet_max) return magnet_max;
    return v;
  }
};