/**
 * @file hash.hpp
 * @author Kiju
 *
 * @brief Hash protocol, common hash functions and helpers.
 */

#pragma once

#include <bit>
#include <concepts>

#include "fr/core/typedefs.hpp"

namespace fr {

struct Hash {
    U64 value;

    static Hash from_raw(U64 v) noexcept {
        return Hash{.value = v};
    }

    /// @brief High 57  bits.
    U64 h57() const noexcept {
        return value >> 7;
    }

    /// @brief Low 7 bits.
    U8 l7() const noexcept {
        return static_cast<U8>(value & 0x7F);
    }
};

namespace impl {
template <typename T>
concept HasMemberHash = requires(const T &v) {
    { v.hash() } noexcept -> std::same_as<Hash>;
};

template <typename T>
concept HasADLHash = requires(const T &v) {
    { hash(v) } noexcept -> std::same_as<Hash>;
};

template <typename T>
concept HasADLHash32 = requires(const T &v) {
    { hash32(v) } noexcept -> std::same_as<Hash>;
};

} // namespace impl

template <typename T>
concept IsHashable = impl::HasMemberHash<T> || impl::HasADLHash<T>;

/// @brief splitmix64 is a fast bijective mixer - excelent to primitives.
inline U64 splitmix64(U64 v) noexcept {
    v ^= v >> 30;
    v *= 0xbf58476d1ce4e5b9ULL;
    v ^= v >> 27;
    v *= 0x94d049bb133111ebULL;
    v ^= v >> 31;
    return v;
}

inline Hash hash(U32 v) noexcept {
    return Hash::from_raw(splitmix64(v));
}

inline Hash hash(U64 v) noexcept {
    return Hash::from_raw(splitmix64(v));
}

inline Hash hash(S32 v) noexcept {
    return Hash::from_raw(splitmix64(static_cast<U64>(v)));
}

inline Hash hash(S64 v) noexcept {
    return Hash::from_raw(splitmix64(static_cast<U64>(v)));
}

inline Hash hash(F32 v) noexcept {
    return Hash::from_raw(splitmix64(std::bit_cast<U32>(v)));
}

inline Hash hash(F64 v) noexcept {
    return Hash::from_raw(splitmix64(std::bit_cast<U64>(v)));
}

inline Hash hash(bool v) noexcept {
    return Hash::from_raw(v ? 1 : 0);
}

/// @brief Wayhash64 is a modern, non-cryptgraphic, general purpose hash function.
inline U64 wyhash64(U64 a, U64 b) noexcept {
    __uint128_t r = static_cast<__uint128_t>(a ^ 0x9e3779b97f4a7c15ULL) *
                    static_cast<__uint128_t>(b ^ 0xe7037ed1a0b428dbULL);

    return static_cast<U64>(r >> 64) ^ static_cast<U64>(r);
}

/// @brief Combines two hash values.
inline Hash combine_hashes(Hash a, Hash b) noexcept {
    return Hash::from_raw(a.value ^
                          (b.value + 0x9e3779b97f4a7c15ULL + (a.value << 6) + (a.value >> 2)));
}

/// @brief Large prime number is a good seed for hash functions.
inline constexpr U64 HASH_SEED = 0xa0761d6478bd642f;
} // namespace fr
