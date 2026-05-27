/**
 * @file part.hpp
 * @author Kiju
 *
 * @brief MAX_PARTS. Interface for a bitset representation of parts owned by a thing.
 */

#pragma once

#include "fr/core/bitset.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"

namespace fr {

constexpr USize MAX_PARTS = 128;

/**
 * @brief Interface for a bitset representation of parts owned by a thing.
 */
class Signature {
public:
    using Storage = Bitset<MAX_PARTS>;

    /**
     * @brief Returns the underlying bitset.
     */
    const Storage &bitset() const noexcept {
        return m_bits;
    }

    /**
     * @brief Destroy all parts - clear all bits.
     */
    void destroy_all() noexcept {
        m_bits.zero_all();
    }

    /**
     * @brief Attach a part type by its TypeIdx.
     * @param idx Type index of the part.
     */
    void insert(TypeIdx tidx) noexcept {
        FR_ASSERT(tidx.idx() < MAX_PARTS, "type idx out of bounds");
        m_bits.one_bit(static_cast<USize>(tidx.idx()));
    }

    /**
     * @brief Detach a part type by its TypeIdx.
     * @param idx Type index of the part.
     */
    void destroy(TypeIdx tidx) noexcept {
        FR_ASSERT(tidx.idx() < MAX_PARTS, "type idx out of bounds");
        m_bits.zero_bit(static_cast<USize>(tidx.idx()));
    }

    /**
     * @brief Check whether a part type is attached by its TypeIdx.
     * @param idx Type index of the part.
     * @return True if attached, false otherwise.
     */
    bool owns(TypeIdx tidx) const noexcept {
        FR_ASSERT(tidx.idx() < MAX_PARTS, "type idx out of bounds");
        return m_bits.check_bit(static_cast<USize>(tidx.idx()));
    }

    /**
     * @brief Check whether any part is attached.
     */
    bool any() const noexcept {
        return m_bits.any();
    }

    /**
     * @brief Check whether none part is attached.
     */
    bool none() const noexcept {
        return m_bits.none();
    }

    /**
     * @brief Equality comparison for signatures.
     */
    friend bool operator==(const Signature &a, const Signature &b) noexcept {
        return a.m_bits == b.m_bits;
    }

    /**
     * @brief Inequality comparison for signatures.
     */
    friend bool operator!=(const Signature &a, const Signature &b) noexcept {
        return !(a == b);
    }

private:
    Storage m_bits{};
};
} // namespace fr
