/**
 * @file thing.hpp
 * @author Kiju
 *
 * @brief Thing represents a universal handle to all game objects. By default, it is 32 bits wide.
 */

#pragma once

#include "fr/core/typedefs.hpp"

namespace fr {

using ThingIdx = U32;
using ThingGen = U32;

/**
 * @brief Thing represents a universal handle to all game objects. By default, it is 32 bits wide.
 *
 * @note The index field is 20 bits wide, and the generation field is 12 bits wide. The nil value
 * for the thing is 0. Nil thing is valid but it acts as a no-op.
 */
struct Thing {
public:
    constexpr Thing() noexcept = default;

    constexpr Thing(ThingIdx idx, ThingGen gen) noexcept
        : m_idx(idx),
          m_gen(gen) {
    }

    /**
     * @brief Returns the index part of the thing.
     */
    constexpr ThingIdx idx() const noexcept {
        return m_idx;
    }

    /**
     * @brief Returns the generation part of the thing.
     */
    constexpr ThingGen gen() const noexcept {
        return m_gen;
    }

    /**
     * @brief Sets the index part of the thing.
     */
    constexpr void set_idx(ThingIdx idx) noexcept {
        m_idx = idx;
    }

    /**
     * @brief Sets the generation part of the thing.
     */
    constexpr void set_gen(ThingGen gen) noexcept {
        m_gen = gen;
    }

    /**
     * @brief Increments generation, skipping 0.
     */
    constexpr void inc_gen() noexcept {
        ++m_gen;
        if (m_gen == 0) {
            m_gen = 1;
        }
    }

    /**
     * @brief Returns the raw value of the thing as U32.
     */
    constexpr U32 as_raw() const noexcept {
        return (m_gen << 20) | m_idx;
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
    constexpr bool is_nil() const noexcept {
        return m_idx == 0 && m_gen == 0;
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

private:
    ThingIdx m_idx : 20 {0};
    ThingGen m_gen : 12 {0};
};
} // namespace fr
