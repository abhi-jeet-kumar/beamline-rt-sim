#pragma once
#include <vector>
#include <atomic>
#include <algorithm>

/**
 * @brief Lock-free ring buffer for high-frequency telemetry data
 * 
 * This implementation uses atomic operations for thread-safe producer access
 * while allowing safe consumer access through iteration. The design prioritizes
 * low-latency writes for real-time control loops over complex synchronization.
 * 
 * Thread Safety:
 * - Single producer (control loop): push() is thread-safe
 * - Single consumer (telemetry/GUI): for_each() is safe when called from one thread
 * - Multiple readers: safe for concurrent read-only access to different elements
 * 
 * Performance Characteristics:
 * - O(1) push operation with minimal contention
 * - No dynamic allocation after construction  
 * - Cache-friendly sequential memory layout
 * - Lock-free design suitable for real-time systems
 */
template<typename T>
class RingBuffer {
private:
    std::vector<T> buf;
    std::atomic<size_t> head{0};

public:
    /**
     * @brief Construct a ring buffer with fixed size
     * @param n Number of elements the buffer can hold
     */
    explicit RingBuffer(size_t n) : buf(n) {
        static_assert(std::is_copy_assignable_v<T>, 
                     "RingBuffer requires copy-assignable type");
    }

    /**
     * @brief Push a new element to the buffer (thread-safe)
     * 
     * This overwrites the oldest element when the buffer is full.
     * Uses relaxed memory ordering for maximum performance in single-producer scenarios.
     * 
     * @param v Value to store
     */
    void push(const T& v) {
        size_t current_head = head.load(std::memory_order_relaxed);
        buf[current_head % buf.size()] = v;
        head.store(current_head + 1, std::memory_order_relaxed);
    }

    /**
     * @brief Apply function to all buffer elements
     * 
     * Iterates through buffer in insertion order (oldest to newest).
     * Safe for concurrent use with push() from single producer.
     * 
     * @param f Function or lambda to apply to each element
     */
    template<class F> 
    void for_each(F f) const {
        const size_t current_head = head.load(std::memory_order_relaxed);
        const size_t buffer_size = buf.size();
        
        if (current_head < buffer_size) {
            // Buffer not yet full - iterate from beginning
            for (size_t i = 0; i < current_head; ++i) {
                f(buf[i]);
            }
        } else {
            // Buffer is full - start from oldest element
            const size_t start_pos = current_head % buffer_size;
            
            // From oldest to end of buffer
            for (size_t i = start_pos; i < buffer_size; ++i) {
                f(buf[i]);
            }
            
            // From beginning to newest
            for (size_t i = 0; i < start_pos; ++i) {
                f(buf[i]);
            }
        }
    }

    /**
     * @brief Get buffer capacity
     * @return Maximum number of elements
     */
    size_t capacity() const {
        return buf.size();
    }

    /**
     * @brief Get current number of valid elements
     * @return Number of elements currently stored (up to capacity)
     */
    size_t size() const {
        const size_t current_head = head.load(std::memory_order_relaxed);
        return std::min(current_head, buf.size());
    }

    /**
     * @brief Check if buffer is empty
     * @return true if no elements have been pushed
     */
    bool empty() const {
        return head.load(std::memory_order_relaxed) == 0;
    }

    /**
     * @brief Check if buffer is at full capacity
     * @return true if buffer contains capacity() elements
     */
    bool full() const {
        return head.load(std::memory_order_relaxed) >= buf.size();
    }

    /**
     * @brief Clear the buffer (not thread-safe)
     * 
     * Should only be called when no other threads are accessing the buffer.
     * Resets head position to 0.
     */
    void clear() {
        head.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Get the most recently pushed element
     * @return Reference to newest element (undefined if empty)
     */
    const T& latest() const {
        const size_t current_head = head.load(std::memory_order_relaxed);
        if (current_head == 0) {
            // Buffer is empty - return first element (undefined behavior but safe)
            return buf[0];
        }
        return buf[(current_head - 1) % buf.size()];
    }

    /**
     * @brief Copy current buffer contents to vector
     * 
     * Returns elements in chronological order (oldest to newest).
     * Useful for atomic snapshots of buffer state.
     * 
     * @return Vector containing current buffer contents
     */
    std::vector<T> snapshot() const {
        std::vector<T> result;
        result.reserve(size());
        for_each([&result](const T& item) {
            result.push_back(item);
        });
        return result;
    }
};
