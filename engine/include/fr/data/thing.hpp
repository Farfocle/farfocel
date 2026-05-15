/**
 * @file thing.hpp
 * @author Kiju
 *
 * @brief Thing represents a universal handle to all game objects. By default, it is 32 bits wide.
 */

#pragma once

#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

using ThingRaw = U32;
using ThingIdx = U32;
using ThingGen = U32;

FR_STATIC_ASSERT(sizeof(ThingRaw) * 2 <= sizeof(ThingIdx) + sizeof(ThingGen),
                 "ThingRaw is too large for the index and generation fields");

/**
 * @brief Thing represents a universal handle to all game objects. By default, it is 32 bits wide.
 *
 * @note The index field is 20 bits wide, and the generation field is 12 bits wide. The nil value
 * for the thing is 0. Nil thing is valid but it acts as a no-op.
 */
struct Thing {
public:
    static constexpr ThingRaw max_index = 0xFFFFF;
    static constexpr ThingRaw max_gen = 0xFFF;

    constexpr Thing() noexcept = default;

    explicit constexpr Thing(ThingRaw idx, ThingRaw gen) noexcept {
        m_thing = (gen << 20) | idx;
    }

    static constexpr Thing from_raw(ThingRaw raw) noexcept {
        return Thing(raw);
    }

    /**
     * @brief Returns the index part of the thing.
     */
    constexpr ThingRaw idx() const noexcept {
        return m_thing & 0xFFFFF;
    }

    constexpr ThingRaw gen() const noexcept {
        return m_thing >> 20;
    }

    /**
     * @brief Sets the index part of the thing.
     */
    constexpr void set_idx(ThingRaw idx) noexcept {
        m_thing = (m_thing & 0xFFF00000) | idx;
    }

    /**
     * @brief Sets the generation part of the thing.
     */
    constexpr void set_gen(ThingRaw gen) noexcept {
        m_thing = (m_thing & 0x000FFFFF) | (gen << 20);
    }

    /**
     * @brief Increments generation, skipping 0.
     */
    constexpr void inc_gen() noexcept {
        set_gen((gen() + 1) % (max_gen + 1));
    }

    /**
     * @brief Returns the raw value of the thing as U32.
     */
    constexpr ThingRaw as_raw() const noexcept {
        return m_thing;
    }

    /**
     * @brief Returns the nil value of the thing.
     */
    static constexpr Thing nil() noexcept {
        return Thing(0);
    }

    /**
     * @brief Returns whether the thing is nil.
     */
    constexpr bool is_nil() const noexcept {
        return m_thing == 0;
    }

    /**
     * @brief Returns whether the thing is nil.
     */
    constexpr bool operator==(const Thing &other) const noexcept {
        return m_thing == other.m_thing;
    }

    /**
     * @brief Returns whether the thing is not nil.
     */
    constexpr bool operator!=(const Thing &other) const noexcept {
        return !(*this == other);
    }

private:
    explicit constexpr Thing(ThingRaw raw) noexcept
        : m_thing(raw) {
    }

    U32 m_thing{0};
};
} // namespace fr
