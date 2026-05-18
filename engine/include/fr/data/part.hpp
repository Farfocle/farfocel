/**
 * @file part.hpp
 * @author Kiju
 *
 * @brief Archetype, some part specific constants.
 */

#pragma once

#include "fr/core/bitset.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typeidx.hpp"

namespace fr {
constexpr USize MAX_PARTS = 128;

/**
 * @brief Represents the parts a thing is made out of. Each bit signals if a specific Part is
 * attatched to the Thing, index to these bits are TypeIdx of the Parts.
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
     * @brief Reset all bits (no parts attached).
     */
    void clear() noexcept {
        m_bits.zero_all();
    }

    /**
     * @brief Attach a part type by its TypeIdx.
     * @param idx Type index of the part.
     */
    void attach(TypeIdx tidx) noexcept {
        FR_ASSERT(tidx < MAX_PARTS, "type idx out of bounds");
        m_bits.one_bit(static_cast<USize>(tidx));
    }

    /**
     * @brief Detach a part type by its TypeIdx.
     * @param idx Type index of the part.
     */
    void detach(TypeIdx tidx) noexcept {
        FR_ASSERT(tidx < MAX_PARTS, "type idx out of bounds");
        m_bits.zero_bit(static_cast<USize>(tidx));
    }

    /**
     * @brief Check whether a part type is attached by its TypeIdx.
     * @param idx Type index of the part.
     * @return True if attached, false otherwise.
     */
    bool check(TypeIdx tidx) const noexcept {
        FR_ASSERT(tidx < MAX_PARTS, "type idx out of bounds");
        return m_bits.check_bit(static_cast<USize>(tidx));
    }

    /**
     * @brief Check whether any part is attached.
     */
    bool any() const noexcept {
        return m_bits.any();
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
