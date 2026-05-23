/**
 * @file bitset.hpp
 * @author Kiju
 *
 * @brief Fixed-size bitset implementation.
 */

#pragma once

#include <bit>
#include <cstddef>
#include <iterator>
#include <utility>

#include "fr/core/array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Concept for bitset iteration callbacks (idx, value).
 */
template <typename Fn>
concept BitsetEachCallback = std::is_invocable_v<Fn, USize, bool>;

/**
 * @brief Concept for bitset iteration callbacks for set bits (idx).
 */
template <typename Fn>
concept BitsetEachOneCallback = std::is_invocable_v<Fn, USize>;

/**
 * @brief Fixed-size collection of bits.
 * @tparam SZ Number of bits.
 *
 * Bitset provides efficient storage and manipulation of a fixed number of bits.
 * It uses a word-based storage (64-bit words) and supports standard bitwise operations.
 */
template <USize SZ>
class Bitset {
public:
    static constexpr USize bit_count = SZ;
    static constexpr USize word_bits = 64;
    static constexpr USize word_count = (SZ + word_bits - 1) / word_bits;

    using Word = U64;
    using Storage = Array<Word, word_count>;

    /**
     * @brief Iterator for bits that are set to 1.
     */
    class OneIterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = USize;
        using pointer = const USize *;
        using reference = const USize &;
        using iterator_category = std::forward_iterator_tag;

        constexpr OneIterator() noexcept = default;
        constexpr OneIterator(const Bitset *owner, USize idx) noexcept
            : m_owner(owner),
              m_idx(idx) {
        }

        constexpr USize operator*() const noexcept {
            return m_idx;
        }

        constexpr OneIterator &operator++() noexcept {
            m_idx = m_owner ? m_owner->do_next_one(m_idx + 1) : SZ;
            return *this;
        }

        constexpr OneIterator operator++(int) noexcept {
            OneIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend constexpr bool operator==(const OneIterator &a, const OneIterator &b) noexcept {
            return a.m_owner == b.m_owner && a.m_idx == b.m_idx;
        }

        friend constexpr bool operator!=(const OneIterator &a, const OneIterator &b) noexcept {
            return !(a == b);
        }

    private:
        const Bitset *m_owner{nullptr};
        USize m_idx{SZ};
    };

    /**
     * @brief Construct a bitset with all bits set to 0.
     */
    constexpr Bitset() noexcept = default;

    /**
     * @brief Construct a bitset with all bits set to 0.
     */
    [[nodiscard]] static constexpr Bitset<SZ> with_zeros() noexcept {
        return Bitset<SZ>();
    }

    /**
     * @brief Construct a bitset with all bits set to 1.
     */
    [[nodiscard]] static constexpr Bitset<SZ> with_ones() noexcept {
        return Bitset<SZ>(~Bitset<SZ>());
    }

    /**
     * @brief Get the number of bits in the bitset.
     */
    [[nodiscard]] static constexpr USize size() noexcept {
        return SZ;
    }

    /**
     * @brief Check if the bitset has zero size.
     */
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return SZ == 0;
    }

    /**
     * @brief Count the number of bits set to 1.
     */
    [[nodiscard]] USize count_ones() const noexcept {
        return do_count_ones();
    }

    /**
     * @brief Count the number of bits set to 0.
     */
    [[nodiscard]] USize count_zeros() const noexcept {
        return do_count_zeros();
    }

    /**
     * @brief Check the value of a bit.
     * @param idx Bit index.
     * @pre idx < size().
     */
    [[nodiscard]] constexpr bool check_bit(USize idx) const noexcept {
        FR_ASSERT(idx < SZ, "bit index out of bounds");

        const USize w = idx / word_bits;
        const USize b = idx % word_bits;

        return (m_words[w] >> b) & 1u;
    }

    /**
     * @brief Set a bit to a specific value.
     * @param idx Bit index.
     * @param value Value to set (default true).
     * @pre idx < size().
     */
    constexpr void set_bit(USize idx, bool value) noexcept {
        FR_ASSERT(idx < SZ, "bit index out of bounds");

        const USize w = idx / word_bits;
        const USize b = idx % word_bits;
        const Word mask = (Word(1) << b);

        if (value) {
            m_words[w] |= mask;
        } else {
            m_words[w] &= ~mask;
        }
    }

    /**
     * @brief Reset a bit to 0.
     * @param idx Bit index.
     * @pre idx < size().
     */
    constexpr void zero_bit(USize idx) noexcept {
        set_bit(idx, false);
    }

    /**
     * @brief Reset a bit to 1.
     * @param idx Bit index.
     * @pre idx < size().
     */
    constexpr void one_bit(USize idx) noexcept {
        set_bit(idx, true);
    }

    /**
     * @brief Flip the value of a bit.
     * @param idx Bit index.
     * @pre idx < size().
     */
    constexpr void flip_bit(USize idx) noexcept {
        FR_ASSERT(idx < SZ, "bit index out of bounds");

        const USize w = idx / word_bits;
        const USize b = idx % word_bits;

        m_words[w] ^= (Word(1) << b);
    }

    /**
     * @brief Sets all bits to a value;
     */
    constexpr void set_all(bool value) noexcept {
        if (value) {
            one_all();
        } else {
            zero_all();
        }
    }

    /**
     * @brief Set all bits to 1.
     */
    constexpr void one_all() noexcept {
        do_one_all();
    }

    /**
     * @brief Reset all bits to 0.
     */
    constexpr void zero_all() noexcept {
        do_zero_all();
    }

    /**
     * @brief Flip all bits.
     */
    constexpr void flip_all() noexcept {
        do_flip_all();
    }

    /**
     * @brief Return a bitset with all bits flipped.
     */
    [[nodiscard]] constexpr Bitset operator~() const noexcept {
        Bitset out = *this;
        out.flip_all();

        return out;
    }

    /**
     * @brief Apply bitwise AND with another bitset.
     */
    constexpr Bitset &operator&=(const Bitset &rhs) noexcept {
        do_apply_and(rhs);
        return *this;
    }

    /**
     * @brief Apply bitwise OR with another bitset.
     */
    constexpr Bitset &operator|=(const Bitset &rhs) noexcept {
        do_apply_or(rhs);
        return *this;
    }

    /**
     * @brief Apply bitwise XOR with another bitset.
     */
    constexpr Bitset &operator^=(const Bitset &rhs) noexcept {
        do_apply_xor(rhs);
        return *this;
    }

    /**
     * @brief Bitwise AND of two bitsets.
     */
    [[nodiscard]] friend constexpr Bitset operator&(const Bitset &a, const Bitset &b) noexcept {
        return do_and(a, b);
    }

    /**
     * @brief Bitwise OR of two bitsets.
     */
    [[nodiscard]] friend constexpr Bitset operator|(const Bitset &a, const Bitset &b) noexcept {
        return do_or(a, b);
    }

    /**
     * @brief Bitwise XOR of two bitsets.
     */
    [[nodiscard]] friend constexpr Bitset operator^(const Bitset &a, const Bitset &b) noexcept {
        return do_xor(a, b);
    }

    /**
     * @brief Checks if any bits are set to 1.
     */
    [[nodiscard]] constexpr bool any() const noexcept {
        for (USize i = 0; i < word_count; ++i) {
            if (m_words[i] != 0) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Checks if all bits are set to 0.
     */
    [[nodiscard]] constexpr bool none() const noexcept {
        return !any();
    }

    /**
     * @brief Equality comparison for bitsets.
     */
    friend constexpr bool operator==(const Bitset &a, const Bitset &b) noexcept {
        for (USize i = 0; i < word_count; ++i) {
            if (a.m_words[i] != b.m_words[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Inequality comparison for bitsets.
     */
    friend constexpr bool operator!=(const Bitset &a, const Bitset &b) noexcept {
        return !(a == b);
    }

    /**
     * @brief Returns an iterator to the first set bit.
     */
    [[nodiscard]] constexpr OneIterator ones_begin() const noexcept {
        return OneIterator(this, do_next_one(0));
    }

    /**
     * @brief Returns an iterator representing the end of set bits.
     */
    [[nodiscard]] constexpr OneIterator ones_end() const noexcept {
        return OneIterator(this, SZ);
    }

    /**
     * @brief Call fn for every bit (idx, value).
     */
    template <BitsetEachCallback Fn>
    constexpr void each(Fn &&fn) const {
        do_each(std::forward<Fn>(fn));
    }

    /**
     * @brief Call fn for every set bit (idx).
     */
    template <BitsetEachOneCallback Fn>
    constexpr void each_one(Fn &&fn) const {
        do_each_one(std::forward<Fn>(fn));
    }

    /**
     * @brief Implementation of shape protocol. Serializes as a string of ones and zeros.
     */
    template <typename Archive>
    void shape(Archive &archive) {
        if constexpr (Archive::action == ArchiveAction::Write) {
            USize sz = SZ;

            archive.prop("@size", sz);

            String s = String::with_capacity(SZ);
            for (USize i = 0; i < SZ; ++i) {
                s.push_back(check_bit(i) ? '1' : '0');
            }

            archive.prop("@value", s);
        } else {
            USize sz = 0;

            archive.prop("@size", sz);
            FR_ASSERT(sz == SZ, "Bitset size mismatch during deserialization");

            String s;
            archive.prop("@value", s);

            zero_all();
            for (USize i = 0; i < std::min(SZ, s.size()); ++i) {
                if (s[i] == '1') {
                    one_bit(i);
                }
            }
        }
    }

private:
    Storage m_words{};

    USize do_count_ones() const noexcept {
        USize count = 0;

        for (USize i = 0; i < word_count; ++i) {
            count += std::popcount(m_words[i]);
        }

        return count;
    }

    USize do_count_zeros() const noexcept {
        return SZ - do_count_ones();
    }

    static constexpr Word do_tail_mask() noexcept {
        if constexpr (word_count == 0) {
            return 0;
        } else {
            constexpr USize tail_bits = SZ % word_bits;
            if constexpr (tail_bits == 0) {
                return ~Word(0);
            } else {
                return (Word(1) << tail_bits) - 1;
            }
        }
    }

    constexpr void do_mask_tail() noexcept {
        if constexpr (word_count > 0) {
            m_words[word_count - 1] &= do_tail_mask();
        }
    }

    constexpr void do_one_all() noexcept {
        for (USize i = 0; i < word_count; ++i) {
            m_words[i] = ~Word(0);
        }

        do_mask_tail();
    }

    constexpr void do_zero_all() noexcept {
        for (USize i = 0; i < word_count; ++i) {
            m_words[i] = 0;
        }
    }

    constexpr void do_flip_all() noexcept {
        for (USize i = 0; i < word_count; ++i) {
            m_words[i] = ~m_words[i];
        }

        do_mask_tail();
    }

    constexpr void do_apply_and(const Bitset &rhs) noexcept {
        for (USize i = 0; i < word_count; ++i) {
            m_words[i] &= rhs.m_words[i];
        }

        do_mask_tail();
    }

    constexpr void do_apply_or(const Bitset &rhs) noexcept {
        for (USize i = 0; i < word_count; ++i) {
            m_words[i] |= rhs.m_words[i];
        }

        do_mask_tail();
    }

    constexpr void do_apply_xor(const Bitset &rhs) noexcept {
        for (USize i = 0; i < word_count; ++i) {
            m_words[i] ^= rhs.m_words[i];
        }
        do_mask_tail();
    }

    static constexpr Bitset do_and(const Bitset &a, const Bitset &b) noexcept {
        Bitset out;
        for (USize i = 0; i < word_count; ++i) {
            out.m_words[i] = a.m_words[i] & b.m_words[i];
        }
        out.do_mask_tail();
        return out;
    }

    static constexpr Bitset do_or(const Bitset &a, const Bitset &b) noexcept {
        Bitset out;

        for (USize i = 0; i < word_count; ++i) {
            out.m_words[i] = a.m_words[i] | b.m_words[i];
        }

        out.do_mask_tail();
        return out;
    }

    static constexpr Bitset do_xor(const Bitset &a, const Bitset &b) noexcept {
        Bitset out;
        for (USize i = 0; i < word_count; ++i) {
            out.m_words[i] = a.m_words[i] ^ b.m_words[i];
        }
        out.do_mask_tail();
        return out;
    }

    constexpr USize do_next_one(USize start) const noexcept {
        if (start >= SZ) {
            return SZ;
        }

        for (USize idx = start; idx < SZ; ++idx) {
            const USize w = idx / word_bits;
            const USize b = idx % word_bits;

            if ((m_words[w] >> b) & 1u) {
                return idx;
            }
        }

        return SZ;
    }

    template <typename Fn>
    constexpr void do_each(Fn &&fn) const {
        for (USize idx = 0; idx < SZ; ++idx) {
            const USize w = idx / word_bits;
            const USize b = idx % word_bits;
            const bool value = ((m_words[w] >> b) & 1u) != 0;

            fn(idx, value);
        }
    }

    template <typename Fn>
    constexpr void do_each_one(Fn &&fn) const {
        for (USize idx = 0; idx < SZ; ++idx) {
            const USize w = idx / word_bits;
            const USize b = idx % word_bits;

            if ((m_words[w] >> b) & 1u) {
                fn(idx);
            }
        }
    }
};

} // namespace fr
