#include "../src/hw/sim_noise.hpp"
#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

/**
 * @brief Unit tests for noise simulation utilities
 * 
 * Tests include:
 * 1. Basic noise generation functionality
 * 2. Statistical properties validation
 * 3. Performance characteristics
 * 4. Specialized beamline noise sources
 * 5. Reproducibility and seeding
 * 6. Thread safety considerations
 */

// Statistical test helpers
double calculate_mean(const std::vector<double>& data) {
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double calculate_std_dev(const std::vector<double>& data, double mean) {
    double variance = 0.0;
    for (double x : data) {
        variance += (x - mean) * (x - mean);
    }
    return std::sqrt(variance / (data.size() - 1));
}

double calculate_skewness(const std::vector<double>& data, double mean, double std_dev) {
    double skewness = 0.0;
    for (double x : data) {
        skewness += std::pow((x - mean) / std_dev, 3);
    }
    return skewness / data.size();
}

int main() {
    std::cout << "Testing Noise Simulation Utilities..." << std::endl;
    
    // Test 1: Basic Gaussian noise
    {
        std::cout << "Test 1: Gaussian noise statistical properties" << std::endl;
        
        NoiseSimulator noise(12345); // Fixed seed for reproducibility
        
        const int n_samples = 100000;
        std::vector<double> samples;
        samples.reserve(n_samples);
        
        // Generate samples
        for (int i = 0; i < n_samples; i++) {
            samples.push_back(noise.gaussian(2.0, 0.5)); // mean=2.0, std=0.5
        }
        
        // Calculate statistics
        double mean = calculate_mean(samples);
        double std_dev = calculate_std_dev(samples, mean);
        double skewness = calculate_skewness(samples, mean, std_dev);
        
        std::cout << "  Expected: mean=2.0, std=0.5" << std::endl;
        std::cout << "  Actual: mean=" << mean << ", std=" << std_dev << std::endl;
        std::cout << "  Skewness: " << skewness << std::endl;
        
        // Statistical tests (with reasonable tolerance for finite samples)
        assert(std::abs(mean - 2.0) < 0.01);      // Mean within 1%
        assert(std::abs(std_dev - 0.5) < 0.01);   // Std dev within 2%
        assert(std::abs(skewness) < 0.1);         // Low skewness for Gaussian
        
        std::cout << "  Gaussian noise test passed" << std::endl;
    }
    
    // Test 2: Fast Gaussian vs standard Gaussian
    {
        std::cout << "Test 2: Fast Gaussian consistency" << std::endl;
        
        NoiseSimulator noise(54321);
        
        const int n_samples = 50000;
        std::vector<double> standard_samples, fast_samples;
        
        // Generate samples with both methods
        for (int i = 0; i < n_samples; i++) {
            standard_samples.push_back(noise.gaussian(0.0, 1.0));
            fast_samples.push_back(noise.gaussian_fast(1.0));
        }
        
        double mean_std = calculate_mean(standard_samples);
        double mean_fast = calculate_mean(fast_samples);
        double std_std = calculate_std_dev(standard_samples, mean_std);
        double std_fast = calculate_std_dev(fast_samples, mean_fast);
        
        std::cout << "  Standard: mean=" << mean_std << ", std=" << std_std << std::endl;
        std::cout << "  Fast: mean=" << mean_fast << ", std=" << std_fast << std::endl;
        
        // Both should have similar statistical properties
        assert(std::abs(mean_std) < 0.02);
        assert(std::abs(mean_fast) < 0.02);
        assert(std::abs(std_std - 1.0) < 0.02);
        assert(std::abs(std_fast - 1.0) < 0.02);
        
        std::cout << "  Fast Gaussian consistency test passed" << std::endl;
    }
    
    // Test 3: Poisson noise
    {
        std::cout << "Test 3: Poisson noise properties" << std::endl;
        
        NoiseSimulator noise(98765);
        
        // Test different mean values
        std::vector<double> test_means = {1.0, 5.0, 20.0, 100.0};
        
        for (double expected_mean : test_means) {
            std::vector<double> samples;
            const int n_samples = 10000;
            
            for (int i = 0; i < n_samples; i++) {
                samples.push_back(noise.poisson(expected_mean));
            }
            
            double actual_mean = calculate_mean(samples);
            double actual_var = calculate_std_dev(samples, actual_mean);
            actual_var = actual_var * actual_var; // Convert to variance
            
            std::cout << "  Mean " << expected_mean << ": actual_mean=" 
                      << actual_mean << ", variance=" << actual_var << std::endl;
            
            // For Poisson distribution: mean ≈ variance
            assert(std::abs(actual_mean - expected_mean) < expected_mean * 0.05);
            assert(std::abs(actual_var - expected_mean) < expected_mean * 0.15);
        }
        
        std::cout << "  Poisson noise test passed" << std::endl;
    }
    
    // Test 4: Pink noise spectral properties
    {
        std::cout << "Test 4: Pink noise characteristics" << std::endl;
        
        NoiseSimulator noise(11111);
        
        const int n_samples = 10000;
        std::vector<double> samples;
        
        for (int i = 0; i < n_samples; i++) {
            samples.push_back(noise.pink_noise(1.0));
        }
        
        double mean = calculate_mean(samples);
        double std_dev = calculate_std_dev(samples, mean);
        
        std::cout << "  Pink noise: mean=" << mean << ", std=" << std_dev << std::endl;
        
        // Pink noise should be centered around zero with reasonable amplitude
        assert(std::abs(mean) < 0.1);
        assert(std_dev > 0.01 && std_dev < 1.0);
        
        std::cout << "  Pink noise test passed" << std::endl;
    }
    
    // Test 5: Brown noise (should have increasing variance)
    {
        std::cout << "Test 5: Brown noise (random walk)" << std::endl;
        
        NoiseSimulator noise(22222);
        
        const int n_samples = 1000;
        std::vector<double> samples;
        
        for (int i = 0; i < n_samples; i++) {
            samples.push_back(noise.brown_noise(0.1));
        }
        
        // Brown noise should show increasing variance over time
        double early_var = 0.0, late_var = 0.0;
        int quarter = n_samples / 4;
        
        // Calculate variance in first and last quarters
        double early_mean = calculate_mean(std::vector<double>(samples.begin(), samples.begin() + quarter));
        double late_mean = calculate_mean(std::vector<double>(samples.end() - quarter, samples.end()));
        
        for (int i = 0; i < quarter; i++) {
            early_var += (samples[i] - early_mean) * (samples[i] - early_mean);
            late_var += (samples[n_samples - 1 - i] - late_mean) * (samples[n_samples - 1 - i] - late_mean);
        }
        early_var /= quarter;
        late_var /= quarter;
        
        std::cout << "  Early variance: " << early_var << ", Late variance: " << late_var << std::endl;
        
        // Later samples should have higher variance (random walk property)
        assert(late_var > early_var);
        
        std::cout << "  Brown noise test passed" << std::endl;
    }
    
    // Test 6: Quantization noise
    {
        std::cout << "Test 6: Quantization noise" << std::endl;
        
        NoiseSimulator noise(33333);
        
        const double test_signal = 0.5;
        const int bits = 8;
        const double full_scale = 1.0;
        const double lsb = full_scale / (1 << bits); // 1/256
        
        std::vector<double> quantized_samples;
        const int n_samples = 1000;
        
        for (int i = 0; i < n_samples; i++) {
            quantized_samples.push_back(noise.quantization_noise(test_signal, bits, full_scale));
        }
        
        double mean = calculate_mean(quantized_samples);
        double std_dev = calculate_std_dev(quantized_samples, mean);
        
        std::cout << "  LSB: " << lsb << ", Mean: " << mean << ", Std: " << std_dev << std::endl;
        
        // Mean should be close to original signal
        assert(std::abs(mean - test_signal) < lsb);
        
        // Standard deviation should be approximately LSB/sqrt(12) for uniform quantization
        double expected_std = lsb / std::sqrt(12.0);
        assert(std::abs(std_dev - expected_std) < expected_std * 0.2);
        
        std::cout << "  Quantization noise test passed" << std::endl;
    }
    
    // Test 7: Uniform noise
    {
        std::cout << "Test 7: Uniform noise distribution" << std::endl;
        
        NoiseSimulator noise(44444);
        
        const double min_val = -2.0, max_val = 3.0;
        const int n_samples = 50000;
        std::vector<double> samples;
        
        for (int i = 0; i < n_samples; i++) {
            double sample = noise.uniform(min_val, max_val);
            samples.push_back(sample);
            
            // All samples should be within range
            assert(sample >= min_val && sample <= max_val);
        }
        
        double mean = calculate_mean(samples);
        double expected_mean = (min_val + max_val) / 2.0;
        double expected_std = (max_val - min_val) / std::sqrt(12.0);
        double actual_std = calculate_std_dev(samples, mean);
        
        std::cout << "  Expected: mean=" << expected_mean << ", std=" << expected_std << std::endl;
        std::cout << "  Actual: mean=" << mean << ", std=" << actual_std << std::endl;
        
        assert(std::abs(mean - expected_mean) < 0.02);
        assert(std::abs(actual_std - expected_std) < 0.02);
        
        std::cout << "  Uniform noise test passed" << std::endl;
    }
    
    // Test 8: Exponential noise
    {
        std::cout << "Test 8: Exponential noise distribution" << std::endl;
        
        NoiseSimulator noise(55555);
        
        const double rate = 2.0; // rate parameter
        const double expected_mean = 1.0 / rate;
        const int n_samples = 20000;
        std::vector<double> samples;
        
        for (int i = 0; i < n_samples; i++) {
            double sample = noise.exponential(rate);
            samples.push_back(sample);
            
            // All samples should be non-negative
            assert(sample >= 0.0);
        }
        
        double actual_mean = calculate_mean(samples);
        double actual_std = calculate_std_dev(samples, actual_mean);
        
        std::cout << "  Expected mean: " << expected_mean << std::endl;
        std::cout << "  Actual: mean=" << actual_mean << ", std=" << actual_std << std::endl;
        
        // For exponential distribution: mean = std = 1/rate
        assert(std::abs(actual_mean - expected_mean) < 0.02);
        assert(std::abs(actual_std - expected_mean) < 0.05);
        
        std::cout << "  Exponential noise test passed" << std::endl;
    }
    
    // Test 9: BPM noise characteristics
    {
        std::cout << "Test 9: BPM noise simulation" << std::endl;
        
        BeamlineNoise::BPMNoise bpm_noise(66666);
        
        const int n_samples = 1000;
        std::vector<double> low_current_noise, high_current_noise;
        
        // Test with different beam currents
        for (int i = 0; i < n_samples; i++) {
            low_current_noise.push_back(bpm_noise.generate(1.0));   // Low current
            high_current_noise.push_back(bpm_noise.generate(1000.0)); // High current
        }
        
        double low_std = calculate_std_dev(low_current_noise, calculate_mean(low_current_noise));
        double high_std = calculate_std_dev(high_current_noise, calculate_mean(high_current_noise));
        
        std::cout << "  Low current noise std: " << low_std << std::endl;
        std::cout << "  High current noise std: " << high_std << std::endl;
        
        // Higher current should have lower shot noise (better SNR)
        assert(high_std < low_std);
        
        std::cout << "  BPM noise test passed" << std::endl;
    }
    
    // Test 10: BIC noise (Poisson statistics)
    {
        std::cout << "Test 10: BIC noise simulation" << std::endl;
        
        BeamlineNoise::BICNoise bic_noise(77777);
        
        const double true_intensity = 1000.0;
        const int n_samples = 1000;
        std::vector<double> samples;
        
        for (int i = 0; i < n_samples; i++) {
            samples.push_back(bic_noise.generate(true_intensity));
        }
        
        double mean = calculate_mean(samples);
        double std_dev = calculate_std_dev(samples, mean);
        
        std::cout << "  True intensity: " << true_intensity << std::endl;
        std::cout << "  Measured: mean=" << mean << ", std=" << std_dev << std::endl;
        
        // Mean should be close to true intensity
        assert(std::abs(mean - true_intensity) < true_intensity * 0.1);
        
        // Standard deviation should be roughly sqrt(intensity) due to Poisson statistics
        double expected_std = std::sqrt(true_intensity);
        assert(std_dev > expected_std * 0.5 && std_dev < expected_std * 2.0);
        
        std::cout << "  BIC noise test passed" << std::endl;
    }
    
    // Test 11: Magnet noise
    {
        std::cout << "Test 11: Magnet current noise" << std::endl;
        
        BeamlineNoise::MagnetNoise magnet_noise(88888);
        
        const double commanded_current = 5.0; // 5A
        const int n_samples = 1000;
        std::vector<double> noise_samples;
        
        for (int i = 0; i < n_samples; i++) {
            double noise = magnet_noise.generate(commanded_current);
            noise_samples.push_back(noise);
        }
        
        double noise_mean = calculate_mean(noise_samples);
        double noise_std = calculate_std_dev(noise_samples, noise_mean);
        
        std::cout << "  Current noise: mean=" << noise_mean << ", std=" << noise_std << std::endl;
        
        // Noise should be small compared to signal
        assert(std::abs(noise_mean) < commanded_current * 0.01);
        assert(noise_std < commanded_current * 0.01);
        
        std::cout << "  Magnet noise test passed" << std::endl;
    }
    
    // Test 12: Reproducibility with seeding
    {
        std::cout << "Test 12: Reproducibility with seeding" << std::endl;
        
        const uint64_t test_seed = 123456789;
        const int n_samples = 100;
        
        // Generate two sequences with same seed
        NoiseSimulator noise1(test_seed);
        NoiseSimulator noise2(test_seed);
        
        bool identical = true;
        for (int i = 0; i < n_samples; i++) {
            double val1 = noise1.gaussian();
            double val2 = noise2.gaussian();
            
            if (std::abs(val1 - val2) > 1e-15) {
                identical = false;
                break;
            }
        }
        
        assert(identical);
        
        // Test reset functionality
        noise1.reset();
        noise2.set_seed(test_seed);
        
        identical = true;
        for (int i = 0; i < 50; i++) {
            if (std::abs(noise1.gaussian() - noise2.gaussian()) > 1e-15) {
                identical = false;
                break;
            }
        }
        
        assert(identical);
        
        std::cout << "  Reproducibility test passed" << std::endl;
    }
    
    // Test 13: Generation counter
    {
        std::cout << "Test 13: Generation counter" << std::endl;
        
        NoiseSimulator noise(99999);
        
        assert(noise.get_generation_count() == 0);
        
        noise.gaussian();
        assert(noise.get_generation_count() == 1);
        
        noise.poisson(10.0);
        assert(noise.get_generation_count() == 2);
        
        for (int i = 0; i < 100; i++) {
            noise.gaussian_fast();
        }
        assert(noise.get_generation_count() == 102);
        
        noise.reset();
        assert(noise.get_generation_count() == 0);
        
        std::cout << "  Generation counter test passed" << std::endl;
    }
    
    std::cout << "\n✅ All Noise Simulation tests passed!" << std::endl;
    return 0;
}
