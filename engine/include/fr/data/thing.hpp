/**
 * @file thing.hpp
 * @author Kiju
 *
 * @brief Thing represents a universal handle to all game objects. By default, it is 32 bits wide.
 */

#pragma once

#include "fr/core/typedefs.hpp"

namespace fr {
/**
 * @brief Thing represents a universal handle to all game objects. By default, it is 32 bits wide.
 *
 * @note The index field is 20 bits wide, and the generation field is 12 bits wide. The nil value
 * for the thing is 0. Nil thing is valid but it acts as a no-op.
 */
struct Thing {
    const U32 idx : 20;
    const U32 gen : 12;

    constexpr Thing(U32 idx, U32 gen) noexcept
        : idx(idx),
          gen(gen) {
    }

    /**
     * @brief Returns the raw value of the thing as U32.
     */
    constexpr U32 as_raw() const noexcept {
        return (gen << 20) | idx;
    }

    /**
     * @brief Returns the nil value of the thing.
     */
    static constexpr Thing nil() noexcept {
        return Thing(0, 0);
    }

    /**
     * @brief Returns whether the thing is nil.
     */
    constexpr bool operator==(const Thing &other) const noexcept {
        return as_raw() == other.as_raw();
    }

    /**
     * @brief Returns whether the thing is not nil.
     */
    constexpr bool operator!=(const Thing &other) const noexcept {
        return !(*this == other);
    }
};
} // namespace fr
