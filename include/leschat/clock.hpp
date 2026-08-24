#pragma once

#include <chrono>
#include <cstdint>

namespace leschat {

inline std::int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

}  // namespace leschat
