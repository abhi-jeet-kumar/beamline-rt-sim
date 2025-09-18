#include "../src/hw/sim_noise.hpp"
#include "stress_test_framework.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
#include <thread>
#include <atomic>

/**
 * @brief Comprehensive stress testing for noise simulation
 * 
 * Tests include:
 * 1. High-frequency noise generation (MHz rates)
 * 2. Multi-threaded noise generation performance
 * 3. Memory pressure resistance
 * 4. Long-term statistical stability
 * 5. Beamline-specific noise performance
 * 6. Real-time noise injection simulation
 * 7. Endurance testing for continuous operation
 */

int main() {
    std::cout << "🔥 COMPREHENSIVE STRESS TESTING: Noise Simulation" << std::endl;
    std::cout << "=================================================" << std::endl;
    
    // Test 1: High-frequency noise generation
    {
        std::cout << "\n🚀 Test 1: High-frequency noise generation (1MHz)" << std::endl;
        
        NoiseSimulator noise(12345);
        StressTest::PerformanceMonitor monitor;
        
        const int iterations = 2000000; // 2M samples
        volatile double sum = 0.0; // Prevent optimization
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            // Mix of different noise types
            double sample = 0.0;
            switch (i % 4) {
                case 0: sample = noise.gaussian_fast(1.0); break;
                case 1: sample = noise.uniform(-1.0, 1.0); break;
                case 2: sample = noise.poisson(10.0); break;
                case 3: sample = noise.exponential(1.0); break;
            }
            sum += sample; // Use the sample
            
            auto end = std::chrono::steady_clock::now();
            double gen_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(gen_time);
            
            // Check for reasonable performance
            if (gen_time > 10.0) { // >10μs is too slow for real-time
                monitor.record_deadline_miss();
            }
        }
        
        monitor.print_statistics("High-frequency Noise Generation");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 1000000); // >1M samples/sec
        assert(stats.p99_us < 5.0); // P99 < 5μs per sample
        assert(stats.deadline_miss_rate < 0.01); // <1% deadline misses
        
        std::cout << "  Total samples generated: " << noise.get_generation_count() << std::endl;
        std::cout << "  Sum (prevent optimization): " << sum << std::endl;
        
        std::cout << "✅ High-frequency generation test PASSED" << std::endl;
    }
    
    // Test 2: Multi-threaded noise generation
    {
        std::cout << "\n🚀 Test 2: Multi-threaded noise generation" << std::endl;
        
        const int num_threads = 8;
        const int samples_per_thread = 500000;
        std::vector<std::thread> threads;
        std::vector<StressTest::PerformanceMonitor> monitors(num_threads);
        std::atomic<uint64_t> total_samples{0};
        
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                NoiseSimulator noise(1000 + t); // Different seed per thread
                uint64_t local_samples = 0;
                volatile double sum = 0.0;
                
                for (int i = 0; i < samples_per_thread; i++) {
                    auto start = std::chrono::steady_clock::now();
                    
                    // Generate various noise types
                    double sample = 0.0;
                    switch (i % 5) {
                        case 0: sample = noise.gaussian(); break;
                        case 1: sample = noise.gaussian_fast(); break;
                        case 2: sample = noise.uniform(-10, 10); break;
                        case 3: sample = noise.poisson(5.0); break;
                        case 4: sample = noise.pink_noise(); break;
                    }
                    sum += sample;
                    local_samples++;
                    
                    auto end = std::chrono::steady_clock::now();
                    double gen_time = std::chrono::duration<double, std::micro>(end - start).count();
                    monitors[t].record_timing(gen_time);
                }
                
                total_samples.fetch_add(local_samples);
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Analyze per-thread performance
        std::cout << "  Thread performance analysis:" << std::endl;
        for (int t = 0; t < num_threads; t++) {
            auto stats = monitors[t].get_statistics();
            std::cout << "    Thread " << t << ": " << stats.throughput_ops_per_sec 
                      << " samples/sec, P99: " << stats.p99_us << "μs" << std::endl;
            
            assert(stats.throughput_ops_per_sec > 200000); // Each thread >200k samples/sec
        }
        
        assert(total_samples.load() == num_threads * samples_per_thread);
        
        std::cout << "✅ Multi-threaded generation test PASSED" << std::endl;
    }
    
    // Test 3: Beamline noise performance under stress
    {
        std::cout << "\n🚀 Test 3: Beamline noise performance" << std::endl;
        
        BeamlineNoise::BPMNoise bpm_noise(2001);
        BeamlineNoise::BICNoise bic_noise(2002);
        BeamlineNoise::MagnetNoise magnet_noise(2003);
        
        StressTest::PerformanceMonitor monitor;
        
        const int iterations = 100000;
        std::vector<double> beam_currents = {1.0, 10.0, 100.0, 1000.0};
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            // Simulate complete beamline noise for one time step
            double beam_current = beam_currents[i % beam_currents.size()];
            
            double bpm_noise_val = bpm_noise.generate(beam_current, 0.001);
            double bic_noise_val = bic_noise.generate(beam_current * 1000);
            double magnet_noise_val = magnet_noise.generate(beam_current * 0.1, 0.001);
            
            // Use the values to prevent optimization
            volatile double total = bpm_noise_val + bic_noise_val + magnet_noise_val;
            (void)total;
            
            auto end = std::chrono::steady_clock::now();
            double gen_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(gen_time);
            
            // Real-time requirement: complete beamline noise in <50μs
            if (gen_time > 50.0) {
                monitor.record_deadline_miss();
            }
        }
        
        monitor.print_statistics("Beamline Noise Generation");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 50000); // >50k complete cycles/sec
        assert(stats.p99_us < 30.0); // P99 < 30μs for complete beamline
        assert(stats.deadline_miss_rate < 0.05); // <5% deadline misses
        
        std::cout << "✅ Beamline noise performance test PASSED" << std::endl;
    }
    
    // Test 4: Statistical stability under CPU stress
    {
        std::cout << "\n🚀 Test 4: Statistical stability under CPU stress" << std::endl;
        
        NoiseSimulator noise(3001);
        StressTest::CPUStressor cpu_stress;
        StressTest::PerformanceMonitor monitor;
        
        cpu_stress.start_stress();
        
        const int n_samples = 100000;
        std::vector<double> samples;
        samples.reserve(n_samples);
        
        for (int i = 0; i < n_samples; i++) {
            auto start = std::chrono::steady_clock::now();
            
            double sample = noise.gaussian(5.0, 2.0); // mean=5, std=2
            samples.push_back(sample);
            
            auto end = std::chrono::steady_clock::now();
            double gen_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(gen_time);
        }
        
        cpu_stress.stop_stress();
        
        // Check statistical properties under stress
        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        double mean = sum / samples.size();
        
        double variance = 0.0;
        for (double x : samples) {
            variance += (x - mean) * (x - mean);
        }
        double std_dev = std::sqrt(variance / (samples.size() - 1));
        
        std::cout << "  Under CPU stress: mean=" << mean << ", std=" << std_dev << std::endl;
        
        // Statistical properties should be maintained even under stress
        assert(std::abs(mean - 5.0) < 0.05);
        assert(std::abs(std_dev - 2.0) < 0.05);
        
        monitor.print_statistics("Gaussian under CPU Stress");
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 100000); // >100k samples/sec under stress
        
        std::cout << "✅ Statistical stability test PASSED" << std::endl;
    }
    
    // Test 5: Memory pressure resistance
    {
        std::cout << "\n🚀 Test 5: Memory pressure resistance" << std::endl;
        
        NoiseSimulator noise(4001);
        StressTest::MemoryStressor mem_stress;
        StressTest::PerformanceMonitor monitor;
        
        // Apply memory pressure
        mem_stress.allocate_memory_mb(400);
        mem_stress.allocate_memory_mb(400); // Total 800MB
        
        const int iterations = 200000;
        volatile double accumulator = 0.0;
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            // Mix of memory-intensive noise operations
            double sample = 0.0;
            switch (i % 3) {
                case 0: sample = noise.pink_noise(1.0); break;    // Uses filter state
                case 1: sample = noise.brown_noise(0.1); break;   // Uses integrator state
                case 2: sample = noise.gaussian_fast(1.0); break; // Uses cached values
            }
            accumulator += sample;
            
            auto end = std::chrono::steady_clock::now();
            double gen_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(gen_time);
        }
        
        mem_stress.free_all();
        monitor.print_statistics("Noise under Memory Pressure");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 100000); // >100k samples/sec under memory pressure
        
        std::cout << "  Accumulator (prevent optimization): " << accumulator << std::endl;
        std::cout << "✅ Memory pressure test PASSED" << std::endl;
    }
    
    // Test 6: Real-time control simulation with noise injection
    {
        std::cout << "\n🚀 Test 6: Real-time control with noise injection" << std::endl;
        
        BeamlineNoise::BPMNoise bpm_noise(5001);
        BeamlineNoise::MagnetNoise mag_noise(5002);
        
        auto control_loop_with_noise = [&]() {
            // Simulate 1kHz control loop with realistic noise
            double beam_current = 100.0; // mA
            
            // Read BPM with noise
            double true_position = 0.1; // mm
            double bpm_reading = true_position + bpm_noise.generate(beam_current, 0.001);
            
            // Simple proportional control
            double error = 0.0 - bpm_reading;
            double command = error * 0.5; // Proportional gain
            
            // Add magnet noise
            double magnet_noise_val = mag_noise.generate(command, 0.001);
            double actual_command = command + magnet_noise_val;
            
            // Verify reasonable values
            return (std::abs(bpm_reading) < 10.0 && std::abs(actual_command) < 10.0);
        };
        
        StressTest::RealtimeStressTest<decltype(control_loop_with_noise)> rt_test(
            control_loop_with_noise,
            std::chrono::microseconds(1000), // 1kHz
            std::chrono::microseconds(1000)  // 1ms deadline
        );
        
        auto results = rt_test.run_test(
            5000,  // 5 seconds
            false, // no CPU stress
            false  // no memory stress
        );
        
        assert(results.passed);
        assert(results.stats.deadline_miss_rate < 0.01); // <1% deadline misses
        
        std::cout << "✅ Real-time control simulation PASSED" << std::endl;
    }
    
    // Test 7: Long-term statistical stability
    {
        std::cout << "\n🚀 Test 7: Long-term statistical stability" << std::endl;
        
        NoiseSimulator noise(6001);
        
        // Test stability over many samples
        const int batch_size = 50000;
        const int num_batches = 20; // 1M total samples
        
        std::vector<double> batch_means, batch_stds;
        
        for (int batch = 0; batch < num_batches; batch++) {
            std::vector<double> samples;
            samples.reserve(batch_size);
            
            for (int i = 0; i < batch_size; i++) {
                samples.push_back(noise.gaussian(0.0, 1.0));
            }
            
            // Calculate batch statistics
            double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
            double mean = sum / samples.size();
            
            double variance = 0.0;
            for (double x : samples) {
                variance += (x - mean) * (x - mean);
            }
            double std_dev = std::sqrt(variance / (samples.size() - 1));
            
            batch_means.push_back(mean);
            batch_stds.push_back(std_dev);
        }
        
        // Analyze long-term stability
        double mean_of_means = std::accumulate(batch_means.begin(), batch_means.end(), 0.0) / batch_means.size();
        double mean_of_stds = std::accumulate(batch_stds.begin(), batch_stds.end(), 0.0) / batch_stds.size();
        
        // Calculate standard deviation of batch means (should be small)
        double variance_of_means = 0.0;
        for (double m : batch_means) {
            variance_of_means += (m - mean_of_means) * (m - mean_of_means);
        }
        double std_of_means = std::sqrt(variance_of_means / (batch_means.size() - 1));
        
        std::cout << "  Long-term statistics over " << (num_batches * batch_size) << " samples:" << std::endl;
        std::cout << "    Mean of means: " << mean_of_means << " (should be ~0)" << std::endl;
        std::cout << "    Mean of std devs: " << mean_of_stds << " (should be ~1)" << std::endl;
        std::cout << "    Std of means: " << std_of_means << " (should be small)" << std::endl;
        
        // Long-term stability requirements
        assert(std::abs(mean_of_means) < 0.01);      // Overall mean stable
        assert(std::abs(mean_of_stds - 1.0) < 0.01); // Standard deviation stable
        assert(std_of_means < 0.01);                 // Low variation between batches
        
        std::cout << "✅ Long-term stability test PASSED" << std::endl;
    }
    
    // Test 8: Endurance test
    {
        std::cout << "\n🚀 Test 8: Endurance test" << std::endl;
        
        NoiseSimulator noise(7001);
        BeamlineNoise::BPMNoise bpm_noise(7002);
        
        auto endurance_test_func = [&]() {
            // Mix of different noise generation operations
            static int counter = 0;
            counter++;
            
            switch (counter % 6) {
                case 0: return noise.gaussian(0, 1) != 0.0;
                case 1: return noise.gaussian_fast(1) != 0.0;
                case 2: return noise.poisson(10) >= 0.0;
                case 3: return noise.pink_noise(1) != 0.0;
                case 4: return noise.uniform(-1, 1) >= -1.0;
                case 5: return bpm_noise.generate(100.0) != 0.0;
            }
            return true;
        };
        
        StressTest::EnduranceTest<decltype(endurance_test_func)> endurance_test(endurance_test_func);
        
        // Run for 30 seconds
        endurance_test.run_for_duration(std::chrono::seconds(30));
        
        // Verify generators are still working
        double final_sample = noise.gaussian();
        assert(std::isfinite(final_sample));
        
        uint64_t total_generated = noise.get_generation_count();
        std::cout << "  Total samples generated during endurance: " << total_generated << std::endl;
        assert(total_generated > 100000); // Should generate many samples
        
        std::cout << "✅ Endurance test PASSED" << std::endl;
    }
    
    std::cout << "\n🎉 ALL NOISE SIMULATION STRESS TESTS PASSED!" << std::endl;
    std::cout << "📊 Noise simulation validated for production real-time use" << std::endl;
    
    return 0;
}
