/**
 * @file algo.hpp
 * @author Kiju
 * @brief This file contains commonly used algos.
 *
 * @detail Following algos are implemented:
 * - Radix Sort (LSD, byte-sized chunks, counting sort)
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/array.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
// ------------------------------------------------------------------ Radix Sort

namespace impl {

constexpr USize RADIX_BUCKETS = 256;

template <typename KeyT>
using RadixKeyUnsigned = std::make_unsigned_t<KeyT>;

/**
 * @brief Convert an integral key to a little-endian byte array.
 * Signed keys have their sign bit flipped so negatives sort before non-negatives.
 */
template <typename KeyT>
Array<U8, sizeof(KeyT)> radix_key_to_bytes(KeyT key) noexcept {
    FR_STATIC_ASSERT(std::is_integral_v<KeyT>, "KeyT must be integral");

    using UT = RadixKeyUnsigned<KeyT>;
    UT value = static_cast<UT>(key);

    if constexpr (std::is_signed_v<KeyT>) {
        constexpr UT sign_bit = UT(1) << ((sizeof(KeyT) * 8) - 1);
        value ^= sign_bit;
    }

    Array<U8, sizeof(KeyT)> bytes{};
    for (USize i = 0; i < sizeof(KeyT); ++i) {
        bytes[i] = static_cast<U8>((value >> (i * 8)) & 0xFFu);
    }

    return bytes;
}

/**
 * @brief Sort byte-array keys in-place via LSD counting sort.
 */
template <USize KeySize>
void radix_sort_raw(Slice<Array<U8, KeySize>> keys) {
    FR_STATIC_ASSERT(KeySize > 0, "KeySize must be non-zero");

    using Key = Array<U8, KeySize>;
    USize n = keys.size();

    DynamicArray<Key> temp;
    temp.grow_default(n);

    Slice<Key> src = keys;
    Slice<Key> dst = temp.slice_mut();

    for (USize chunk = 0; chunk < KeySize; ++chunk) {
        Array<USize, RADIX_BUCKETS> counts{};

        for (USize i = 0; i < n; ++i) {
            ++counts[src[i][chunk]];
        }

        USize acc = 0;
        for (USize b = 0; b < RADIX_BUCKETS; ++b) {
            USize c = counts[b];
            counts[b] = acc;
            acc += c;
        }

        for (USize i = 0; i < n; ++i) {
            U8 byte = src[i][chunk];
            dst[counts[byte]++] = src[i];
        }

        Slice<Key> tmp = src;
        src = dst;
        dst = tmp;
    }

    if (src.data() != keys.data()) {
        for (USize i = 0; i < n; ++i) {
            keys[i] = src[i];
        }
    }
}

/**
 * @brief Generate the permutation that stably sorts byte-array keys (LSD counting sort).
 * out_indices[i] = original index of the element at sorted position i.
 */
template <USize KeySize>
void radix_argsort_raw(Slice<Array<U8, KeySize>> keys, Slice<USize> out_indices) {
    FR_STATIC_ASSERT(KeySize > 0, "KeySize must be non-zero");
    FR_ASSERT(keys.size() == out_indices.size(), "keys and out_indices must have the same size");

    USize n = keys.size();

    for (USize i = 0; i < n; ++i) {
        out_indices[i] = i;
    }

    DynamicArray<USize> temp;
    temp.grow_default(n);

    Slice<USize> src = out_indices;
    Slice<USize> dst = temp.slice_mut();

    for (USize chunk = 0; chunk < KeySize; ++chunk) {
        Array<USize, RADIX_BUCKETS> counts{};

        for (USize i = 0; i < n; ++i) {
            ++counts[keys[src[i]][chunk]];
        }

        USize acc = 0;
        for (USize b = 0; b < RADIX_BUCKETS; ++b) {
            USize c = counts[b];
            counts[b] = acc;
            acc += c;
        }

        for (USize i = 0; i < n; ++i) {
            U8 byte = keys[src[i]][chunk];
            dst[counts[byte]++] = src[i];
        }

        Slice<USize> tmp = src;
        src = dst;
        dst = tmp;
    }

    if (src.data() != out_indices.data()) {
        for (USize i = 0; i < n; ++i) {
            out_indices[i] = src[i];
        }
    }
}

} // namespace impl

// ------------------------------------------------------------------ Concepts

template <typename KeyT>
concept RadixIntegralKey = std::is_integral_v<KeyT>;

template <typename T, typename KeyFn>
concept RadixKeyFn =
    std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> &&
    // @note Not requiring nothrow invocable — too cumbersome in practice.
    std::is_integral_v<std::invoke_result_t<KeyFn, const T &>>;

// ----------------------------------------------------------------- apply_permutation

/**
 * @brief Reorder items in-place according to a permutation.
 * After the call, items[i] == old_items[perm[i]].
 */
template <typename Item>
void apply_permutation(Slice<Item> items, Slice<USize> perm) {
    FR_ASSERT(items.size() == perm.size(), "items and perm must have the same size");

    USize n = items.size();
    DynamicArray<Item> reordered;
    reordered.reserve(n);

    for (USize i = 0; i < n; ++i) {
        reordered.push_back(std::move(items[perm[i]]));
    }

    for (USize i = 0; i < n; ++i) {
        items[i] = std::move(reordered[i]);
    }
}

// -------------------------------------------------------------- radix_sort_raw

/**
 * @brief Sort byte-array keys in-place.
 */
template <USize KeySize>
void radix_sort_raw(Slice<Array<U8, KeySize>> keys) {
    if (keys.is_empty()) {
        return;
    }

    impl::radix_sort_raw(keys);
}

/**
 * @brief Generate the permutation that sorts byte-array keys.
 * out_indices[i] = original index of the element at sorted position i.
 */
template <USize KeySize>
void radix_argsort_raw(Slice<Array<U8, KeySize>> keys, Slice<USize> out_indices) {
    if (keys.is_empty()) {
        return;
    }

    impl::radix_argsort_raw(keys, out_indices);
}

// ----------------------------------------------------------------- radix_sort

/**
 * @brief Sort integral keys in-place.
 */
template <RadixIntegralKey Key>
void radix_sort(Slice<Key> keys) {
    if (keys.is_empty()) {
        return;
    }

    DynamicArray<Array<U8, sizeof(Key)>> key_bytes;
    key_bytes.reserve(keys.size());
    for (USize i = 0; i < keys.size(); ++i) {
        key_bytes.push_back(impl::radix_key_to_bytes(keys[i]));
    }

    impl::radix_sort_raw(key_bytes.slice_mut());

    for (USize i = 0; i < keys.size(); ++i) {
        using UT = impl::RadixKeyUnsigned<Key>;
        UT value = 0;

        for (USize b = 0; b < sizeof(Key); ++b) {
            value |= (UT(key_bytes[i][b]) << (b * 8));
        }

        if constexpr (std::is_signed_v<Key>) {
            constexpr UT sign_bit = UT(1) << ((sizeof(Key) * 8) - 1);
            value ^= sign_bit;
        }

        keys[i] = static_cast<Key>(value);
    }
}

/**
 * @brief Generate the permutation that sorts integral keys.
 * out_indices[i] = original index of the element at sorted position i.
 */
template <RadixIntegralKey Key>
void radix_argsort(Slice<Key> keys, Slice<USize> out_indices) {
    if (keys.is_empty()) {
        return;
    }

    FR_ASSERT(out_indices.size() == keys.size(), "keys and out_indices must have the same size");

    DynamicArray<Array<U8, sizeof(Key)>> key_bytes;
    key_bytes.reserve(keys.size());
    for (USize i = 0; i < keys.size(); ++i) {
        key_bytes.push_back(impl::radix_key_to_bytes(keys[i]));
    }

    impl::radix_argsort_raw(key_bytes.slice_mut(), out_indices);
}

// -------------------------------------------------------------- radix_sort_key

/**
 * @brief Sort items in-place by an integral key extracted via key_fn.
 */
template <typename Item, typename KeyFn>
    requires RadixKeyFn<Item, KeyFn>
void radix_sort_key(Slice<Item> items, KeyFn key_fn) {
    if (items.is_empty()) {
        return;
    }

    using Key = std::invoke_result_t<KeyFn, const Item &>;

    DynamicArray<Array<U8, sizeof(Key)>> key_bytes;
    key_bytes.reserve(items.size());
    for (USize i = 0; i < items.size(); ++i) {
        key_bytes.push_back(impl::radix_key_to_bytes<Key>(key_fn(items[i])));
    }

    DynamicArray<USize> perm;
    perm.reserve(items.size());
    for (USize i = 0; i < items.size(); ++i) {
        perm.push_back(USize{0});
    }

    impl::radix_argsort_raw(key_bytes.slice_mut(), perm.slice_mut());
    apply_permutation(items, perm.slice_mut());
}

/**
 * @brief Generate the permutation that sorts items by an integral key extracted via key_fn.
 * out_indices[i] = original index of the element at sorted position i.
 */
template <typename Item, typename KeyFn>
    requires RadixKeyFn<Item, KeyFn>
void radix_argsort_key(Slice<Item> items, KeyFn key_fn, Slice<USize> out_indices) {
    if (items.is_empty())
        return;
    FR_ASSERT(out_indices.size() == items.size(), "items and out_indices must have the same size");

    using Key = std::invoke_result_t<KeyFn, const Item &>;

    DynamicArray<Array<U8, sizeof(Key)>> key_bytes;
    key_bytes.reserve(items.size());
    for (USize i = 0; i < items.size(); ++i) {
        key_bytes.push_back(impl::radix_key_to_bytes<Key>(key_fn(items[i])));
    }

    impl::radix_argsort_raw(key_bytes.slice_mut(), out_indices);
}

} // namespace fr
