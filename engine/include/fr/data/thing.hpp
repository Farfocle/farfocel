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

constexpr USize THING_IDX_BITS = 20;
constexpr USize THING_GEN_BITS = 12;
constexpr USize THING_RAW_BITS = THING_IDX_BITS + THING_GEN_BITS;
constexpr ThingIdx THING_MAX_IDX = 0xFFFFF;
constexpr ThingGen THING_MAX_GEN = 0xFFF;
constexpr USize MAX_THINGS = 1 << THING_IDX_BITS;

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
    // ------------------------------------------------------------ Constructors

    constexpr Thing() noexcept = default;
    explicit constexpr Thing(ThingIdx idx, ThingGen gen) noexcept {
        m_thing = (gen << THING_IDX_BITS) | idx;
    }

    static constexpr Thing from_raw(ThingRaw raw) noexcept {
        return Thing(raw);
    }

    // ----------------------------------------------------------------- Methods

    /**
     * @brief Returns the index part of the thing.
     */
    constexpr ThingIdx idx() const noexcept {
        return m_thing & 0xFFFFF;
    }

    constexpr ThingGen gen() const noexcept {
        return m_thing >> THING_IDX_BITS;
    }

    /**
     * @brief Sets the index part of the thing.
     */
    constexpr void set_idx(ThingIdx idx) noexcept {
        m_thing = (m_thing & 0xFFF00000) | idx;
    }

    /**
     * @brief Sets the generation part of the thing.
     */
    constexpr void set_gen(ThingGen gen) noexcept {
        m_thing = (m_thing & 0x000FFFFF) | (gen << THING_IDX_BITS);
    }

    /**
     * @brief Increments generation, wraps around, skipping 0.
     */
    constexpr void inc_gen() noexcept {
        set_gen((gen() + 1) % (THING_MAX_GEN + 1));
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
