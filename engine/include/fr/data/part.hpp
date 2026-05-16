/**
 * @file part.hpp
 * @author Kiju
 *
 * @brief Archetype, some part specific constants.
 */

#pragma once

#include "fr/core/bitset.hpp"
#include "fr/core/macros.hpp"
#include "fr/data/typeidx.hpp"

namespace fr {
constexpr USize MAX_PARTS = 128;

/**
 * @brief Represents the parts a thing is made out of. Each bit signals if a specific Part is
 * attatched to the Thing, index to these bits are TypeIdx of the Parts.
 */
class Signature {
public:
    using BitsetType = Bitset<MAX_PARTS>;

    /**
     * @brief Returns the underlying bitset.
     */
    const BitsetType &bitset() const noexcept {
        return m_bits;
    }

    /**
     * @brief Attach a part type by its TypeIdx.
     * @param idx Type index of the part.
     */
    void attach_part_by_idx(TypeIdx idx) noexcept {
        FR_ASSERT(idx < MAX_PARTS, "type idx out of bounds");
        m_bits.one_bit(static_cast<USize>(idx));
    }

    /**
     * @brief Detach a part type by its TypeIdx.
     * @param idx Type index of the part.
     */
    void detach_part_by_idx(TypeIdx idx) noexcept {
        FR_ASSERT(idx < MAX_PARTS, "type idx out of bounds");
        m_bits.zero_bit(static_cast<USize>(idx));
    }

    /**
     * @brief Check whether a part type is attached by its TypeIdx.
     * @param idx Type index of the part.
     * @return True if attached, false otherwise.
     */
    bool check_by_idx(TypeIdx idx) const noexcept {
        FR_ASSERT(idx < MAX_PARTS, "type idx out of bounds");
        return m_bits.check_bit(static_cast<USize>(idx));
    }

    /**
     * @brief Attach a part type.
     */
    template <typename T>
    void attach_part() noexcept {
        attach_part_by_idx(impl::DataTypeIdxGen::gen<T>());
    }

    /**
     * @brief Detach a part type.
     */
    template <typename T>
    void detach_part() noexcept {
        detach_part_by_idx(impl::DataTypeIdxGen::gen<T>());
    }

    /**
     * @brief Check whether a part type is attached.
     */
    template <typename T>
    bool check() const noexcept {
        return check_by_idx(impl::DataTypeIdxGen::gen<T>());
    }

private:
    BitsetType m_bits{};
};
} // namespace fr

