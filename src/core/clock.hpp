#pragma once
#include <chrono>
#include <thread>

/**
 * @brief High-precision periodic clock for real-time control loops
 * 
 * Provides deterministic timing for control loops by using std::this_thread::sleep_until
 * which is more accurate than std::this_thread::sleep_for for periodic operations.
 * 
 * The clock automatically compensates for drift by maintaining the next wake time
 * based on the original start time plus multiples of the period.
 */
struct PeriodicClock {
    using clock = std::chrono::steady_clock;
    
    std::chrono::nanoseconds period;
    clock::time_point next;

    /**
     * @brief Construct a new Periodic Clock
     * @param p Period between clock ticks in nanoseconds
     */
    explicit PeriodicClock(std::chrono::nanoseconds p) 
        : period(p), next(clock::now() + p) {}

    /**
     * @brief Wait until the next scheduled tick
     * 
     * This method sleeps until the next scheduled wake time and then
     * advances the schedule by one period. This approach prevents
     * drift accumulation that would occur with repeated sleep_for calls.
     */
    void wait_next() {
        std::this_thread::sleep_until(next);
        next += period;
    }

    /**
     * @brief Get the current period
     * @return Period in nanoseconds
     */
    std::chrono::nanoseconds get_period() const {
        return period;
    }

    /**
     * @brief Update the period (takes effect after next wait_next call)
     * @param new_period New period in nanoseconds
     */
    void set_period(std::chrono::nanoseconds new_period) {
        period = new_period;
        // Reset next wake time to avoid large jumps
        next = clock::now() + period;
    }

    /**
     * @brief Get time until next scheduled wake
     * @return Duration until next wake time
     */
    std::chrono::nanoseconds time_to_next() const {
        auto now = clock::now();
        if (next <= now) {
            return std::chrono::nanoseconds::zero();
        }
        return std::chrono::duration_cast<std::chrono::nanoseconds>(next - now);
    }
};
