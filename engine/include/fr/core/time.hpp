/**
 * @author Kiju
 *
 *
 */

#pragma once

#include <chrono>

#include "fr/core/typedefs.hpp"

namespace fr::time {
[[nodiscard]] inline U64 get_system_now_ms() noexcept {
    const auto now = std::chrono::system_clock::now();
    return static_cast<U64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

[[nodiscard]] inline U64 get_system_now_ns() noexcept {
    const auto now = std::chrono::system_clock::now();
    return static_cast<U64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
}

[[nodiscard]] inline U64 get_steady_now_ms() noexcept {
    const auto now = std::chrono::steady_clock::now();
    return static_cast<U64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

[[nodiscard]] inline U64 get_steady_now_ns() noexcept {
    const auto now = std::chrono::steady_clock::now();
    return static_cast<U64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
}
} // namespace fr::time
