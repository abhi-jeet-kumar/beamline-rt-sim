#include "../src/core/ring_buffer.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <numeric>
#include <chrono>

/**
 * @brief Test RingBuffer functionality, thread safety, and performance
 * 
 * This test suite verifies:
 * 1. Basic operations (push, size, capacity, etc.)
 * 2. Overflow behavior (circular overwriting)
 * 3. Iterator functionality (for_each)
 * 4. Thread safety between producer and consumer
 * 5. Performance characteristics for real-time use
 */

// Simple test data structure
struct TestData {
    int value;
    double timestamp;
    
    TestData() : value(0), timestamp(0.0) {}
    TestData(int v, double t) : value(v), timestamp(t) {}
    
    bool operator==(const TestData& other) const {
        return value == other.value && timestamp == other.timestamp;
    }
};

int main() {
    std::cout << "Testing RingBuffer functionality..." << std::endl;
    
    // Test 1: Basic operations
    {
        std::cout << "Test 1: Basic operations" << std::endl;
        
        RingBuffer<int> buf(5);
        
        // Initial state
        assert(buf.capacity() == 5);
        assert(buf.size() == 0);
        assert(buf.empty());
        assert(!buf.full());
        
        // Add some elements
        buf.push(10);
        buf.push(20);
        buf.push(30);
        
        assert(buf.size() == 3);
        assert(!buf.empty());
        assert(!buf.full());
        assert(buf.latest() == 30);
        
        // Fill to capacity
        buf.push(40);
        buf.push(50);
        
        assert(buf.size() == 5);
        assert(buf.full());
        assert(buf.latest() == 50);
        
        std::cout << "  Basic operations test passed" << std::endl;
    }
    
    // Test 2: Overflow behavior (circular buffer)
    {
        std::cout << "Test 2: Overflow behavior" << std::endl;
        
        RingBuffer<int> buf(3);
        
        // Fill buffer
        buf.push(1);
        buf.push(2);
        buf.push(3);
        
        // Verify contents in order
        std::vector<int> contents;
        buf.for_each([&contents](int x) { contents.push_back(x); });
        assert(contents == std::vector<int>({1, 2, 3}));
        
        // Add more elements (should overwrite oldest)
        buf.push(4);  // overwrites 1
        buf.push(5);  // overwrites 2
        
        assert(buf.size() == 3);  // Still at capacity
        assert(buf.latest() == 5);
        
        // Check new contents (should be 3, 4, 5 in chronological order)
        contents.clear();
        buf.for_each([&contents](int x) { contents.push_back(x); });
        assert(contents == std::vector<int>({3, 4, 5}));
        
        std::cout << "  Overflow behavior test passed" << std::endl;
    }
    
    // Test 3: Complex data types
    {
        std::cout << "Test 3: Complex data types" << std::endl;
        
        RingBuffer<TestData> buf(4);
        
        buf.push(TestData(100, 1.5));
        buf.push(TestData(200, 2.5));
        buf.push(TestData(300, 3.5));
        
        assert(buf.size() == 3);
        assert(buf.latest().value == 300);
        assert(buf.latest().timestamp == 3.5);
        
        // Test for_each with complex data
        double sum_timestamps = 0.0;
        int sum_values = 0;
        buf.for_each([&](const TestData& td) {
            sum_timestamps += td.timestamp;
            sum_values += td.value;
        });
        
        assert(sum_timestamps == 7.5);  // 1.5 + 2.5 + 3.5
        assert(sum_values == 600);      // 100 + 200 + 300
        
        std::cout << "  Complex data types test passed" << std::endl;
    }
    
    // Test 4: Snapshot functionality
    {
        std::cout << "Test 4: Snapshot functionality" << std::endl;
        
        RingBuffer<int> buf(4);
        buf.push(10);
        buf.push(20);
        buf.push(30);
        
        auto snapshot = buf.snapshot();
        assert(snapshot.size() == 3);
        assert(snapshot == std::vector<int>({10, 20, 30}));
        
        // Add more and test snapshot after overflow
        buf.push(40);
        buf.push(50);  // Should overwrite 10
        
        snapshot = buf.snapshot();
        assert(snapshot.size() == 4);
        assert(snapshot == std::vector<int>({20, 30, 40, 50}));
        
        std::cout << "  Snapshot functionality test passed" << std::endl;
    }
    
    // Test 5: Clear functionality
    {
        std::cout << "Test 5: Clear functionality" << std::endl;
        
        RingBuffer<int> buf(3);
        buf.push(1);
        buf.push(2);
        buf.push(3);
        
        assert(buf.size() == 3);
        assert(buf.full());
        
        buf.clear();
        
        assert(buf.size() == 0);
        assert(buf.empty());
        assert(!buf.full());
        
        // Should work normally after clear
        buf.push(100);
        assert(buf.size() == 1);
        assert(buf.latest() == 100);
        
        std::cout << "  Clear functionality test passed" << std::endl;
    }
    
    // Test 6: Thread safety (producer-consumer pattern)
    {
        std::cout << "Test 6: Thread safety" << std::endl;
        
        RingBuffer<int> buf(1000);
        std::atomic<bool> producer_done{false};
        std::atomic<int> consumer_reads{0};
        
        const int num_items = 5000;  // Reduced for faster test
        
        // Producer thread - pushes sequential numbers
        std::thread producer([&buf, &producer_done, num_items]() {
            for (int i = 0; i < num_items; ++i) {
                buf.push(i);
            }
            producer_done.store(true);
        });
        
        // Consumer thread - counts elements via for_each (with timeout)
        std::thread consumer([&buf, &producer_done, &consumer_reads]() {
            auto start_time = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::seconds(5);  // 5 second timeout
            
            while (!producer_done.load()) {
                int count = 0;
                buf.for_each([&count](int) { count++; });
                consumer_reads.fetch_add(count);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                
                // Safety timeout
                if (std::chrono::steady_clock::now() - start_time > timeout) {
                    break;
                }
            }
            
            // Final read
            int count = 0;
            buf.for_each([&count](int) { count++; });
            consumer_reads.fetch_add(count);
        });
        
        producer.join();
        consumer.join();
        
        // Verify buffer is not corrupted
        assert(buf.size() <= buf.capacity());
        
        // Should have latest elements
        if (!buf.empty()) {
            assert(buf.latest() == num_items - 1);
        }
        
        std::cout << "  Thread safety test passed" << std::endl;
        std::cout << "  Consumer performed " << consumer_reads.load() 
                  << " total element reads" << std::endl;
    }
    
    // Test 7: Performance benchmark
    {
        std::cout << "Test 7: Performance benchmark" << std::endl;
        
        RingBuffer<double> buf(10000);
        const int num_operations = 1000000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Push operations
        for (int i = 0; i < num_operations; ++i) {
            buf.push(static_cast<double>(i) * 1.5);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        double ns_per_push = static_cast<double>(duration.count()) / num_operations;
        
        std::cout << "  Performance: " << ns_per_push << " ns per push operation" << std::endl;
        std::cout << "  Throughput: " << (1e9 / ns_per_push) << " pushes per second" << std::endl;
        
        // Should be very fast (target: < 100 ns per operation on modern hardware)
        assert(ns_per_push < 1000.0); // Less than 1 microsecond per push
        
        // Test for_each performance
        start = std::chrono::high_resolution_clock::now();
        
        volatile double sum = 0.0;  // Prevent optimization
        buf.for_each([&sum](double x) { sum += x; });
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        double ns_per_read = static_cast<double>(duration.count()) / buf.size();
        std::cout << "  For_each: " << ns_per_read << " ns per element read" << std::endl;
        
        std::cout << "  Performance benchmark passed" << std::endl;
    }
    
    std::cout << "\n✅ All RingBuffer tests passed!" << std::endl;
    return 0;
}
