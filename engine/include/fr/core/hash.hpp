/**
 * @file hash.hpp
 * @author Kiju
 *
 * @brief Hash protocol, common hash functions and helpers.
 */

#pragma once

#include <bit>
#include <concepts>

#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Represents a 64-bit hash value with split parts for hash table indexing.
 *
 * This implementation follows the design where the hash is split into H1 (high bits)
 * for indexing and H2 (low 7 bits) for control byte comparison, similar to Abseil/SwissTable.
 */
struct Hash {
    U64 value{};

    /**
     * @brief Create a Hash object from a raw 64-bit value.
     * @param v Raw hash value.
     * @return Hash object.
     */
    static Hash from_raw(U64 v) noexcept {
        return Hash{.value = v};
    }

    /**
     * @brief Returns the high 57 bits of the hash.
     * @note Used for primary bucket indexing (H1).
     */
    U64 h57() const noexcept {
        return value >> 7;
    }

    /**
     * @brief Returns the low 7 bits of the hash.
     * @note Used for intra-bucket filtering (H2).
     */
    U8 l7() const noexcept {
        return static_cast<U8>(value & 0x7F);
    }
};

/**
 * @brief Represents a 32-bit hash value.
 */
struct Hash32 {
    U32 value{0};

    /**
     * @brief Create a Hash32 object from a raw 32-bit value.
     * @param v Raw hash value.
     * @return Hash32 object.
     */
    static Hash32 from_raw(U32 v) noexcept {
        return Hash32{.value = v};
    }
};

namespace impl {
/**
 * @brief splitmix64 is a fast bijective mixer.
 * @param v Input value.
 * @return Mixed 64-bit value.
 * @note Excellent for hashing primitive types.
 */
inline U64 splitmix64(U64 v) noexcept {
    v ^= v >> 30;
    v *= 0xbf58476d1ce4e5b9ULL;
    v ^= v >> 27;
    v *= 0x94d049bb133111ebULL;
    v ^= v >> 31;
    return v;
}
} // namespace impl

/**
 * @brief Hash a 32-bit unsigned integer.
 */
inline Hash hash(U32 v) noexcept {
    return Hash::from_raw(impl::splitmix64(v));
}

/**
 * @brief Hash a 64-bit unsigned integer.
 */
inline Hash hash(U64 v) noexcept {
    return Hash::from_raw(impl::splitmix64(v));
}

/**
 * @brief Hash a 32-bit signed integer.
 */
inline Hash hash(S32 v) noexcept {
    return Hash::from_raw(impl::splitmix64(static_cast<U64>(v)));
}

/**
 * @brief Hash a 64-bit signed integer.
 */
inline Hash hash(S64 v) noexcept {
    return Hash::from_raw(impl::splitmix64(static_cast<U64>(v)));
}

/**
 * @brief Hash a 32-bit floating-point number.
 */
inline Hash hash(F32 v) noexcept {
    return Hash::from_raw(impl::splitmix64(std::bit_cast<U32>(v)));
}

/**
 * @brief Hash a 64-bit floating-point number.
 */
inline Hash hash(F64 v) noexcept {
    return Hash::from_raw(impl::splitmix64(std::bit_cast<U64>(v)));
}

/**
 * @brief Hash a boolean value.
 */
inline Hash hash(bool v) noexcept {
    return Hash::from_raw(v ? 1 : 0);
}

/**
 * @brief Wyhash64 is a modern, non-cryptographic, general purpose hash function.
 * @param a First component.
 * @param b Second component.
 * @return Mixed 64-bit value.
 */
inline U64 wyhash64(U64 a, U64 b) noexcept {
    __uint128_t r = static_cast<__uint128_t>(a ^ 0x9e3779b97f4a7c15ULL) *
                    static_cast<__uint128_t>(b ^ 0xe7037ed1a0b428dbULL);

    return static_cast<U64>(r >> 64) ^ static_cast<U64>(r);
}

/**
 * @brief Combines two hash values into one.
 * @param a First hash.
 * @param b Second hash.
 * @return Combined hash.
 */
inline Hash combine_hashes(Hash a, Hash b) noexcept {
    return Hash::from_raw(a.value ^
                          (b.value + 0x9e3779b97f4a7c15ULL + (a.value << 6) + (a.value >> 2)));
}

/**
 * @brief Large prime number seed for hash functions.
 */
inline constexpr U64 HASH_SEED = 0xa0761d6478bd642f;

/**
 * @brief Simple and fast hash for a block of memory.
 * @param ptr Pointer to the data.
 * @param len Length in bytes.
 * @return Computed hash.
 * @pre @p ptr must be non-null if @p len > 0.
 */
inline Hash hash_bytes(const void *ptr, USize len) noexcept {
    FR_ASSERT(len == 0 || ptr != nullptr, "pointer must be non-null if size is non-zero");

    U64 h = HASH_SEED;
    const U8 *data = static_cast<const U8 *>(ptr);

    for (USize i = 0; i < len; ++i) {
        h = (h ^ data[i]) * 0x100000001b3ULL;
    }

    return Hash::from_raw(h);
}

namespace impl {
/**
 * @brief Concept for types that have a .hash() member function.
 */
template <typename T>
concept HasMemberHash = requires(const T &v) {
    { v.hash() } noexcept -> std::same_as<Hash>;
};

/**
 * @brief Concept for types that have a .hash32() member function.
 */
template <typename T>
concept HasMemberHash32 = requires(const T &v) {
    { v.hash32() } noexcept -> std::same_as<Hash>;
};

/**
 * @brief Concept for types that have a hash(T) overload in their namespace.
 */
template <typename T>
concept HasADLHash = requires(const T &v) {
    { hash(v) } noexcept -> std::same_as<Hash>;
};

/**
 * @brief Concept for types that have a hash32(T) overload in their namespace.
 */
template <typename T>
concept HasADLHash32 = requires(const T &v) {
    { hash32(v) } noexcept -> std::same_as<Hash>;
};
} // namespace impl

/**
 * @brief Concept for types that can be hashed using the fr::Hash protocol.
 */
template <typename T>
concept IsHashable = impl::HasMemberHash<T> || impl::HasADLHash<T>;

/**
 * @brief Invokes the appropriate hash function for the given value.
 * @tparam T Type of the value.
 * @param value The value to hash.
 * @return Computed hash.
 * @pre @p T must satisfy IsHashable.
 */
template <typename T>
inline Hash call_hash(const T &value) noexcept {
    if constexpr (impl::HasMemberHash<T>) {
        return value.hash();
    } else if constexpr (impl::HasADLHash<T>) {
        return hash(value);
    } else {
        FR_STATIC_ASSERT(false, "value is not hashable: provide either a .hash() member function "
                                "or a hash(T) overload in the same namespace as T.");
    }
}
} // namespace fr
