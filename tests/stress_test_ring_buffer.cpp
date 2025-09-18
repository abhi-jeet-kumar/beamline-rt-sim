#include "../src/core/ring_buffer.hpp"
#include "stress_test_framework.hpp"
#include <cassert>
#include <iostream>
#include <random>

/**
 * @brief Comprehensive stress testing for RingBuffer
 * 
 * Tests include:
 * 1. High-throughput producer stress (millions of ops/sec)
 * 2. Producer-consumer stress under system load
 * 3. Memory pressure resistance
 * 4. Thread contention and scalability
 * 5. Data integrity under extreme load
 * 6. Long-term stability testing
 */

int main() {
    std::cout << "🔥 COMPREHENSIVE STRESS TESTING: RingBuffer" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    // Test 1: High-throughput producer stress
    {
        std::cout << "\n🚀 Test 1: High-throughput producer stress" << std::endl;
        
        RingBuffer<uint64_t> buffer(10000);
        StressTest::PerformanceMonitor monitor;
        
        const int iterations = 10000000; // 10 million operations
        auto start_time = std::chrono::steady_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            auto op_start = std::chrono::steady_clock::now();
            
            buffer.push(static_cast<uint64_t>(i));
            
            auto op_end = std::chrono::steady_clock::now();
            double op_time_us = std::chrono::duration<double, std::micro>(op_end - op_start).count();
            monitor.record_timing(op_time_us);
        }
        
        monitor.print_statistics("High-throughput Producer");
        auto stats = monitor.get_statistics();
        
        // Performance requirements
        assert(stats.throughput_ops_per_sec > 1000000); // >1M ops/sec
        assert(stats.p99_us < 10.0); // P99 < 10μs per operation
        assert(buffer.size() == buffer.capacity()); // Buffer should be full
        
        std::cout << "✅ High-throughput stress test PASSED" << std::endl;
    }
    
    // Test 2: Producer-consumer stress under CPU load
    {
        std::cout << "\n🚀 Test 2: Producer-consumer under CPU stress" << std::endl;
        
        RingBuffer<int> buffer(1000);
        StressTest::CPUStressor cpu_stress;
        StressTest::PerformanceMonitor producer_monitor, consumer_monitor;
        
        std::atomic<bool> test_running{true};
        std::atomic<uint64_t> items_produced{0};
        std::atomic<uint64_t> items_consumed{0};
        std::atomic<uint64_t> data_integrity_errors{0};
        
        cpu_stress.start_stress();
        
        // Producer thread
        std::thread producer([&]() {
            int value = 0;
            while (test_running.load()) {
                auto start = std::chrono::steady_clock::now();
                
                buffer.push(value++);
                items_produced++;
                
                auto end = std::chrono::steady_clock::now();
                double op_time = std::chrono::duration<double, std::micro>(end - start).count();
                producer_monitor.record_timing(op_time);
                
                if (value % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        });
        
        // Consumer thread
        std::thread consumer([&]() {
            int last_value = -1;
            while (test_running.load() || !buffer.empty()) {
                auto start = std::chrono::steady_clock::now();
                
                buffer.for_each([&](int value) {
                    // Check monotonic increase (allowing for wrap-around in circular buffer)
                    if (last_value >= 0 && value < last_value && (last_value - value) < 900) {
                        data_integrity_errors++;
                    }
                    last_value = value;
                    items_consumed++;
                });
                
                auto end = std::chrono::steady_clock::now();
                double op_time = std::chrono::duration<double, std::micro>(end - start).count();
                consumer_monitor.record_timing(op_time);
                
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
        
        // Run test for 10 seconds
        std::this_thread::sleep_for(std::chrono::seconds(10));
        test_running = false;
        
        producer.join();
        consumer.join();
        cpu_stress.stop_stress();
        
        producer_monitor.print_statistics("Producer under CPU stress");
        consumer_monitor.print_statistics("Consumer under CPU stress");
        
        std::cout << "  Items produced: " << items_produced.load() << std::endl;
        std::cout << "  Items consumed: " << items_consumed.load() << std::endl;
        std::cout << "  Data integrity errors: " << data_integrity_errors.load() << std::endl;
        
        // Performance and correctness requirements
        assert(items_produced.load() > 100000); // Should produce >100k items in 10s
        assert(data_integrity_errors.load() == 0); // No data corruption
        
        auto producer_stats = producer_monitor.get_statistics();
        assert(producer_stats.p99_us < 100.0); // P99 < 100μs even under stress
        
        std::cout << "✅ Producer-consumer stress test PASSED" << std::endl;
    }
    
    // Test 3: Memory pressure resistance
    {
        std::cout << "\n🚀 Test 3: Memory pressure resistance" << std::endl;
        
        RingBuffer<double> buffer(5000);
        StressTest::MemoryStressor mem_stress;
        StressTest::PerformanceMonitor monitor;
        
        // Allocate significant memory pressure
        mem_stress.allocate_memory_mb(200); // 200MB
        mem_stress.allocate_memory_mb(200); // Total 400MB
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0.0, 1000.0);
        
        const int iterations = 100000;
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::steady_clock::now();
            
            double value = dist(gen);
            buffer.push(value);
            
            auto end = std::chrono::steady_clock::now();
            double op_time = std::chrono::duration<double, std::micro>(end - start).count();
            monitor.record_timing(op_time);
        }
        
        mem_stress.free_all();
        monitor.print_statistics("Memory Pressure Test");
        
        auto stats = monitor.get_statistics();
        assert(stats.throughput_ops_per_sec > 500000); // >500k ops/sec under memory pressure
        
        std::cout << "✅ Memory pressure test PASSED" << std::endl;
    }
    
    // Test 4: Thread scalability test
    {
        std::cout << "\n🚀 Test 4: Thread scalability test" << std::endl;
        
        RingBuffer<std::pair<int, int>> buffer(10000);
        const int num_producers = 4;
        const int items_per_producer = 50000;
        
        std::atomic<uint64_t> total_items_produced{0};
        std::vector<std::thread> producers;
        std::vector<StressTest::PerformanceMonitor> monitors(num_producers);
        
        // Start multiple producer threads
        for (int t = 0; t < num_producers; t++) {
            producers.emplace_back([&, t]() {
                for (int i = 0; i < items_per_producer; i++) {
                    auto start = std::chrono::steady_clock::now();
                    
                    buffer.push(std::make_pair(t, i)); // thread_id, item_id
                    total_items_produced++;
                    
                    auto end = std::chrono::steady_clock::now();
                    double op_time = std::chrono::duration<double, std::micro>(end - start).count();
                    monitors[t].record_timing(op_time);
                }
            });
        }
        
        // Wait for all producers to complete
        for (auto& thread : producers) {
            thread.join();
        }
        
        // Analyze per-thread performance
        std::cout << "  Thread performance analysis:" << std::endl;
        for (int t = 0; t < num_producers; t++) {
            auto stats = monitors[t].get_statistics();
            std::cout << "    Thread " << t << ": " << stats.throughput_ops_per_sec 
                      << " ops/sec, P99: " << stats.p99_us << "μs" << std::endl;
            
            assert(stats.throughput_ops_per_sec > 100000); // Each thread >100k ops/sec
        }
        
        assert(total_items_produced.load() == num_producers * items_per_producer);
        assert(buffer.size() == buffer.capacity()); // Buffer should be full
        
        std::cout << "✅ Thread scalability test PASSED" << std::endl;
    }
    
    // Test 5: Data integrity under extreme stress
    {
        std::cout << "\n🚀 Test 5: Data integrity under extreme stress" << std::endl;
        
        struct TestData {
            uint64_t id;
            uint32_t checksum;
            double value;
            
            TestData(uint64_t i = 0) : id(i), value(i * 3.14159) {
                checksum = static_cast<uint32_t>(id ^ static_cast<uint64_t>(value * 1000));
            }
            
            bool is_valid() const {
                uint32_t expected = static_cast<uint32_t>(id ^ static_cast<uint64_t>(value * 1000));
                return checksum == expected;
            }
        };
        
        RingBuffer<TestData> buffer(1000);
        StressTest::CPUStressor cpu_stress;
        StressTest::MemoryStressor mem_stress;
        
        cpu_stress.start_stress();
        mem_stress.allocate_memory_mb(100);
        
        std::atomic<uint64_t> corruption_count{0};
        std::atomic<bool> test_running{true};
        
        // Producer thread
        std::thread producer([&]() {
            uint64_t id = 0;
            while (test_running.load()) {
                buffer.push(TestData(id++));
                if (id % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        });
        
        // Consumer thread checking data integrity
        std::thread consumer([&]() {
            while (test_running.load()) {
                buffer.for_each([&](const TestData& data) {
                    if (!data.is_valid()) {
                        corruption_count++;
                    }
                });
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
        
        // Run for 5 seconds
        std::this_thread::sleep_for(std::chrono::seconds(5));
        test_running = false;
        
        producer.join();
        consumer.join();
        
        cpu_stress.stop_stress();
        mem_stress.free_all();
        
        std::cout << "  Data corruption events: " << corruption_count.load() << std::endl;
        assert(corruption_count.load() == 0); // No data corruption allowed
        
        std::cout << "✅ Data integrity stress test PASSED" << std::endl;
    }
    
    // Test 6: Endurance test
    {
        std::cout << "\n🚀 Test 6: Endurance test" << std::endl;
        
        RingBuffer<int> buffer(1000);
        
        auto test_function = [&buffer]() {
            static int counter = 0;
            buffer.push(counter++);
            
            // Occasionally read data
            if (counter % 100 == 0) {
                volatile int sum = 0;
                buffer.for_each([&sum](int value) { sum += value; });
            }
        };
        
        StressTest::EnduranceTest<decltype(test_function)> endurance_test(test_function);
        
        // Run for 60 seconds
        endurance_test.run_for_duration(std::chrono::seconds(60));
        
        std::cout << "✅ Endurance test PASSED" << std::endl;
    }
    
    std::cout << "\n🎉 ALL RINGBUFFER STRESS TESTS PASSED!" << std::endl;
    std::cout << "📊 RingBuffer validated for high-frequency real-time telemetry" << std::endl;
    
    return 0;
}
