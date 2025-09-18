#pragma once
#include <random>
#include <chrono>
#include <cmath>
#include <array>
#include <algorithm>
#include <atomic>

/**
 * @brief High-performance noise simulation for realistic hardware modeling
 * 
 * Provides various noise types commonly found in beamline instrumentation:
 * - Gaussian white noise (thermal, electronic)
 * - Poisson noise (particle counting statistics)  
 * - 1/f (flicker) noise (long-term drift)
 * - Quantization noise (ADC effects)
 * - Colored noise (correlated noise sources)
 * 
 * Optimized for real-time operation at kHz rates with minimal computational overhead.
 */
class NoiseSimulator {
private:
    mutable std::mt19937_64 rng_;                    ///< High-quality random number generator
    mutable std::normal_distribution<double> normal_; ///< Gaussian distribution
    mutable std::uniform_real_distribution<double> uniform_; ///< Uniform distribution
    
    // 1/f noise generation state
    mutable std::array<double, 16> pink_noise_state_{}; ///< Pink noise filter state
    mutable double brown_noise_state_{0.0};         ///< Brown noise integrator state
    
    // Performance optimization
    mutable bool has_spare_normal_{false};          ///< Box-Muller optimization
    mutable double spare_normal_{0.0};              ///< Cached normal random
    
    // Thread safety
    mutable std::atomic<uint64_t> generation_count_{0}; ///< Total samples generated
    
public:
    /**
     * @brief Construct noise simulator with optional seed
     * @param seed Random seed (0 = use random device)
     */
    explicit NoiseSimulator(uint64_t seed = 0) 
        : rng_(seed == 0 ? std::random_device{}() : seed)
        , normal_(0.0, 1.0)
        , uniform_(0.0, 1.0) 
    {
        // Initialize pink noise filter
        std::fill(pink_noise_state_.begin(), pink_noise_state_.end(), 0.0);
    }
    
    /**
     * @brief Generate Gaussian white noise
     * @param mean Mean value
     * @param std_dev Standard deviation
     * @return Gaussian-distributed random value
     */
    double gaussian(double mean = 0.0, double std_dev = 1.0) const {
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        return mean + std_dev * normal_(rng_);
    }
    
    /**
     * @brief Generate fast Gaussian noise using Box-Muller with caching
     * @param std_dev Standard deviation (mean = 0)
     * @return Gaussian random value
     */
    double gaussian_fast(double std_dev = 1.0) const {
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        
        if (has_spare_normal_) {
            has_spare_normal_ = false;
            return spare_normal_ * std_dev;
        }
        
        // Box-Muller transform
        double u = uniform_(rng_);
        double v = uniform_(rng_);
        double mag = std_dev * std::sqrt(-2.0 * std::log(u));
        
        spare_normal_ = mag * std::cos(2.0 * M_PI * v);
        has_spare_normal_ = true;
        
        return mag * std::sin(2.0 * M_PI * v);
    }
    
    /**
     * @brief Generate Poisson-distributed noise (for counting statistics)
     * @param mean Expected count rate
     * @return Poisson-distributed integer as double
     */
    double poisson(double mean) const {
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        
        if (mean < 30.0) {
            // Use Knuth's algorithm for small means
            double limit = std::exp(-mean);
            double product = uniform_(rng_);
            int count = 0;
            
            while (product > limit) {
                count++;
                product *= uniform_(rng_);
            }
            return static_cast<double>(count);
        } else {
            // Use normal approximation for large means
            return std::max(0.0, gaussian(mean, std::sqrt(mean)));
        }
    }
    
    /**
     * @brief Generate pink (1/f) noise
     * @param amplitude Noise amplitude
     * @return Pink noise sample
     */
    double pink_noise(double amplitude = 1.0) const {
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        
        // Paul Kellett's refined pink noise algorithm
        double white = gaussian_fast();
        
        // Apply series of first-order filters
        pink_noise_state_[0] = 0.99886 * pink_noise_state_[0] + white * 0.0555179;
        pink_noise_state_[1] = 0.99332 * pink_noise_state_[1] + white * 0.0750759;
        pink_noise_state_[2] = 0.96900 * pink_noise_state_[2] + white * 0.1538520;
        pink_noise_state_[3] = 0.86650 * pink_noise_state_[3] + white * 0.3104856;
        pink_noise_state_[4] = 0.55000 * pink_noise_state_[4] + white * 0.5329522;
        pink_noise_state_[5] = -0.7616 * pink_noise_state_[5] - white * 0.0168980;
        
        double pink = pink_noise_state_[0] + pink_noise_state_[1] + pink_noise_state_[2] + 
                     pink_noise_state_[3] + pink_noise_state_[4] + pink_noise_state_[5] + 
                     pink_noise_state_[6] + white * 0.5362;
        
        pink_noise_state_[6] = white * 0.115926;
        
        return pink * amplitude * 0.05; // Scale to reasonable amplitude
    }
    
    /**
     * @brief Generate brown (Brownian/random walk) noise
     * @param step_size Step size for random walk
     * @return Brown noise sample (integrated white noise)
     */
    double brown_noise(double step_size = 1.0) const {
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        brown_noise_state_ += gaussian_fast() * step_size;
        return brown_noise_state_;
    }
    
    /**
     * @brief Generate quantization noise (ADC effects)
     * @param signal Input signal value
     * @param bits ADC resolution in bits
     * @param full_scale_range Full scale range of ADC
     * @return Signal with quantization noise added
     */
    double quantization_noise(double signal, int bits, double full_scale_range = 1.0) const {
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        
        double lsb = full_scale_range / (1ULL << bits);  // Least significant bit
        double quantized = std::round(signal / lsb) * lsb;
        
        // Add uniform quantization noise ±0.5 LSB
        return quantized + uniform_(rng_) * lsb - 0.5 * lsb;
    }
    
    /**
     * @brief Generate uniform random noise
     * @param min Minimum value
     * @param max Maximum value
     * @return Uniformly distributed random value
     */
    double uniform(double min, double max) const {
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        return min + (max - min) * uniform_(rng_);
    }
    
    /**
     * @brief Generate exponential noise (e.g., for time intervals)
     * @param rate Rate parameter (1/mean)
     * @return Exponentially distributed value
     */
    double exponential(double rate = 1.0) const {
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        return -std::log(uniform_(rng_)) / rate;
    }
    
    /**
     * @brief Generate correlated noise using first-order filter
     * @param input_noise Input white noise
     * @param correlation_time Time constant for correlation
     * @param dt Time step
     * @return Correlated noise sample
     */
    double correlated_noise(double input_noise, double correlation_time, double dt) const {
        static thread_local double prev_output = 0.0;
        generation_count_.fetch_add(1, std::memory_order_relaxed);
        
        double alpha = dt / (correlation_time + dt);
        double output = alpha * input_noise + (1.0 - alpha) * prev_output;
        prev_output = output;
        return output;
    }
    
    /**
     * @brief Reset internal state (for reproducible sequences)
     */
    void reset() {
        std::fill(pink_noise_state_.begin(), pink_noise_state_.end(), 0.0);
        brown_noise_state_ = 0.0;
        has_spare_normal_ = false;
        generation_count_.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total number of samples generated
     */
    uint64_t get_generation_count() const {
        return generation_count_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Seed the random number generator
     * @param seed New seed value
     */
    void set_seed(uint64_t seed) {
        rng_.seed(seed);
        reset();
    }
};

/**
 * @brief Specialized noise sources for beamline components
 */
namespace BeamlineNoise {
    
    /**
     * @brief BPM (Beam Position Monitor) noise characteristics
     */
    class BPMNoise {
    private:
        NoiseSimulator noise_;
        double thermal_noise_level_{0.001};     ///< Thermal noise in mm
        double electronics_noise_level_{0.0005}; ///< Electronics noise in mm
        double vibration_amplitude_{0.01};      ///< Mechanical vibration amplitude
        double vibration_frequency_{50.0};      ///< Vibration frequency in Hz
        mutable double phase_{0.0};             ///< Vibration phase
        
    public:
        explicit BPMNoise(uint64_t seed = 0) : noise_(seed) {}
        
        /**
         * @brief Generate realistic BPM noise
         * @param beam_current Beam current (affects shot noise)
         * @param dt Time step for vibration calculation
         * @return Total noise in mm
         */
        double generate(double beam_current = 100.0, double dt = 0.001) const {
            // Thermal noise (independent of beam current)
            double thermal = noise_.gaussian_fast(thermal_noise_level_);
            
            // Electronics noise
            double electronics = noise_.gaussian_fast(electronics_noise_level_);
            
            // Shot noise (scales with 1/sqrt(current))
            double shot_noise_level = 0.01 / std::sqrt(std::max(beam_current, 1.0));
            double shot = noise_.gaussian_fast(shot_noise_level);
            
            // Mechanical vibrations (correlated)
            phase_ += 2.0 * M_PI * vibration_frequency_ * dt;
            double vibration = vibration_amplitude_ * std::sin(phase_) + 
                              noise_.gaussian_fast(vibration_amplitude_ * 0.1);
            
            // 1/f noise for long-term drift
            double drift = noise_.pink_noise(0.002);
            
            return thermal + electronics + shot + vibration + drift;
        }
        
        void set_noise_levels(double thermal, double electronics, double vibration) {
            thermal_noise_level_ = thermal;
            electronics_noise_level_ = electronics;
            vibration_amplitude_ = vibration;
        }
    };
    
    /**
     * @brief BIC (Beam Intensity Counter) noise characteristics
     */
    class BICNoise {
    private:
        NoiseSimulator noise_;
        double dark_current_{10.0};             ///< Dark current in counts
        double gain_variation_{0.02};           ///< Gain stability (2%)
        
    public:
        explicit BICNoise(uint64_t seed = 0) : noise_(seed) {}
        
        /**
         * @brief Generate realistic BIC noise
         * @param true_intensity True beam intensity
         * @return Measured intensity with noise
         */
        double generate(double true_intensity) const {
            // Poisson counting statistics (dominant for low intensities)
            double base_counts = true_intensity + dark_current_;
            double poisson_counts = noise_.poisson(base_counts);
            
            // Gain variation (multiplicative noise)
            double gain_factor = 1.0 + noise_.gaussian_fast(gain_variation_);
            
            // Electronics noise (additive)
            double electronics = noise_.gaussian_fast(std::sqrt(base_counts) * 0.1);
            
            return std::max(0.0, (poisson_counts + electronics) * gain_factor - dark_current_);
        }
        
        void set_dark_current(double dark) { dark_current_ = dark; }
        void set_gain_variation(double variation) { gain_variation_ = variation; }
    };
    
    /**
     * @brief Magnet power supply noise characteristics
     */
    class MagnetNoise {
    private:
        NoiseSimulator noise_;
        double current_stability_{1e-5};        ///< Current stability (ppm)
        double ripple_amplitude_{1e-4};         ///< Power supply ripple
        double ripple_frequency_{100.0};        ///< Ripple frequency in Hz
        mutable double ripple_phase_{0.0};      ///< Ripple phase
        
    public:
        explicit MagnetNoise(uint64_t seed = 0) : noise_(seed) {}
        
        /**
         * @brief Generate magnet current noise
         * @param commanded_current Commanded current value
         * @param dt Time step
         * @return Current noise in Amperes
         */
        double generate(double commanded_current, double dt = 0.001) const {
            // Current stability noise
            double stability = noise_.gaussian_fast(std::abs(commanded_current) * current_stability_);
            
            // Power supply ripple
            ripple_phase_ += 2.0 * M_PI * ripple_frequency_ * dt;
            double ripple = ripple_amplitude_ * std::abs(commanded_current) * std::sin(ripple_phase_);
            
            // 1/f noise for drift
            double drift = noise_.pink_noise(std::abs(commanded_current) * 1e-6);
            
            // Quantization noise (16-bit DAC)
            double full_scale = 10.0; // ±10A range
            double quantized = noise_.quantization_noise(commanded_current, 16, full_scale);
            
            return (quantized - commanded_current) + stability + ripple + drift;
        }
        
        void set_current_stability(double stability) { current_stability_ = stability; }
        void set_ripple(double amplitude, double frequency) {
            ripple_amplitude_ = amplitude;
            ripple_frequency_ = frequency;
        }
    };
    
} // namespace BeamlineNoise
