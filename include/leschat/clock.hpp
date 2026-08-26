#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>

namespace leschat {

inline std::atomic<std::int64_t> time_offset_ms{0};

inline std::int64_t system_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline std::int64_t current_time_ms() {
    return system_time_ms() + time_offset_ms.load(std::memory_order_relaxed);
}

inline std::int64_t current_time_offset_ms() {
    return time_offset_ms.load(std::memory_order_relaxed);
}

inline void set_time_offset_ms(std::int64_t offset_ms) {
    time_offset_ms.store(offset_ms, std::memory_order_relaxed);
}

inline bool set_system_time_ms(std::int64_t timestamp_ms) {
    if (timestamp_ms < 0) {
        return false;
    }

    const timespec timestamp{
        .tv_sec = static_cast<time_t>(timestamp_ms / 1000),
        .tv_nsec = static_cast<long>((timestamp_ms % 1000) * 1000000)
    };
    if (clock_settime(CLOCK_REALTIME, &timestamp) != 0) {
        return false;
    }

    set_time_offset_ms(0);
    return true;
}

}  // namespace leschat
