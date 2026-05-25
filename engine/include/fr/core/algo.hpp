/**
 * @file algo.hpp
 * @author Kiju
 * @brief This file contains commonly used algos.
 *
 * @detail Following algos are implemented:
 * - Radix Sort (byte sized chunks)
 */

#pragma once

#include <type_traits>

#include "fr/core/array.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
// ------------------------------------------------------------------ Radix Sort

namespace impl {
/**
 * @brief Fixed bucket count for byte-wise radix passes.
 */
constexpr USize RADIX_BUCKETS = 256;

/**
 * @brief Unsigned counterpart of a radix key.
 *
 * @tparam KeyT Integral key type.
 */
template <typename KeyT>
using RadixKeyUnsigned = std::make_unsigned_t<KeyT>;

/**
 * @brief Convert an integral key into a byte array suitable for radix sorting.
 *
 * @details
 * Keys are converted to little-endian byte order. For signed keys the sign bit is flipped so
 * negative values sort before non-negative values during unsigned radix passes.
 *
 * @tparam KeyT Integral key type.
 * @param key Input key.
 * @return Byte array representation of the key.
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

    // @todo This is slow. May need template specialization, it gets quite complex with
    // `fr::Array`.
    Array<U8, sizeof(KeyT)> bytes{};
    for (USize i = 0; i < sizeof(KeyT); ++i) {
        bytes[i] = static_cast<U8>((value >> (i * 8)) & 0xFFu);
    }

    return bytes;
}

/**
 * @brief Radix-sort key/index pairs in-place using byte-sized chunks.
 *
 * @tparam KeySize Key length in bytes.
 * @param keys Byte-array keys.
 * @param indices Indices paired with keys.
 */
template <USize KeySize>
void radix_sort_pairs_inplace(Slice<Array<U8, KeySize>> keys, Slice<USize> indices) {

    FR_STATIC_ASSERT(KeySize > 0, "KeySize must be non-zero");
    FR_STATIC_ASSERT(KeySize <= 128,
                     "KeySize must be equal or smaller than 128; be reasonable fellow programmer");
    FR_ASSERT(keys.size() == indices.size(), "keys and indices have to be the same exact size");

    using Key = Array<U8, KeySize>;
    Array<DynamicArray<Key>, RADIX_BUCKETS> key_buckets;
    Array<DynamicArray<USize>, RADIX_BUCKETS> idx_buckets;

    for (USize chunk = 0; chunk < KeySize; ++chunk) {
        for (USize i = 0; i < keys.size(); ++i) {
            U8 byte = keys[i][chunk];
            key_buckets[byte].push_back(keys[i]);
            idx_buckets[byte].push_back(indices[i]);
        }

        USize curr = 0;
        for (USize i = 0; i < RADIX_BUCKETS; ++i) {
            auto &key_bucket = key_buckets[i];
            auto &idx_bucket = idx_buckets[i];

            FR_ASSERT(key_bucket.size() == idx_bucket.size(),
                      "INVARIANCE; bucket sizes have to match");

            for (USize j = 0; j < key_bucket.size(); ++j) {
                keys[curr] = key_bucket[j];
                indices[curr] = idx_bucket[j];
                ++curr;
            }

            key_bucket.clear();
            idx_bucket.clear();
        }
    }
}

/**
 * @brief Radix-sort byte-array keys in-place using byte-sized chunks.
 *
 * @tparam KeySize Key length in bytes.
 * @param keys Byte-array keys.
 */
template <USize KeySize>
void radix_sort_inplace(Slice<Array<U8, KeySize>> keys) {

    FR_STATIC_ASSERT(KeySize > 0, "KeySize must be non-zero");
    FR_STATIC_ASSERT(KeySize <= 128,
                     "KeySize must be equal or smaller than 128; be reasonable fellow programmer");

    using Key = Array<U8, KeySize>;
    Array<DynamicArray<Key>, RADIX_BUCKETS> key_buckets;

    for (USize chunk = 0; chunk < KeySize; ++chunk) {
        for (USize i = 0; i < keys.size(); ++i) {
            U8 byte = keys[i][chunk];
            key_buckets[byte].push_back(keys[i]);
        }

        USize curr = 0;
        for (USize i = 0; i < RADIX_BUCKETS; ++i) {
            auto &key_bucket = key_buckets[i];

            for (USize j = 0; j < key_bucket.size(); ++j) {
                keys[curr] = key_bucket[j];
                ++curr;
            }

            key_bucket.clear();
        }
    }
}
} // namespace impl

/**
 * @brief Radix-sort byte-array keys in-place using byte-sized chunks.
 *
 * @tparam KeySize Key length in bytes.
 * @param keys Byte-array keys.
 */
template <USize KeySize>
void radix_sort_inplace(Slice<Array<U8, KeySize>> keys) {
    impl::radix_sort_inplace(keys);
}

/**
 * @brief Concept for integral radix keys.
 */
template <typename KeyT>
concept RadixIntegralKey = std::is_integral_v<KeyT>;

/**
 * @brief Radix-sort integral keys in-place.
 *
 * @details
 * Keys are converted to byte arrays internally. Signed keys are ordered by flipping the sign bit.
 *
 * @tparam Key Integral key type.
 * @param keys Keys to sort.
 */
template <RadixIntegralKey Key>
void radix_sort_inplace(Slice<Key> keys) {

    if (keys.is_empty()) {
        return;
    }

    DynamicArray<Array<U8, sizeof(Key)>> key_bytes;
    key_bytes.reserve(keys.size());

    for (USize i = 0; i < keys.size(); ++i) {
        key_bytes.push_back(impl::radix_key_to_bytes(keys[i]));
    }

    auto key_slice = key_bytes.slice_mut();
    impl::radix_sort_inplace(key_slice);

    for (USize i = 0; i < key_bytes.size(); ++i) {
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
 * @brief Concept for extracting integral keys from complex items.
 *
 * @tparam T Item type.
 * @tparam KeyFn Callable type that returns an integral key.
 */
template <typename T, typename KeyFn>
concept RadixKeyFn =
    std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> &&
    // @note This callback would normally nothrow invocable, but this is very cumbersome to work
    // with, so I decided to simply ignore it :)
    // ----> std::is_nothrow_invocable_v<KeyFn, const T &> &&
    std::is_integral_v<std::invoke_result_t<KeyFn, const T &>>;

/**
 * @brief Radix-sort complex items by an integral key in-place.
 *
 * @details
 * The algorithm extracts all keys, radix-sorts them while tracking indices, and then reorders
 * the original items in a single pass.
 *
 * @tparam Item Item type.
 * @tparam KeyFn Key extraction function type.
 * @param items Items to sort.
 * @param key_fn Callable that returns an integral key for each item.
 */
template <typename Item, typename KeyFn>
    requires RadixKeyFn<Item, KeyFn>
void radix_sort_by_key_inplace(Slice<Item> items, KeyFn key_fn) {
    if (items.is_empty()) {
        return;
    }

    using Key = std::invoke_result_t<KeyFn, const Item &>;

    DynamicArray<Array<U8, sizeof(Key)>> keys;
    DynamicArray<USize> indices;

    keys.reserve(items.size());
    indices.reserve(items.size());

    for (USize i = 0; i < items.size(); ++i) {
        keys.push_back(impl::radix_key_to_bytes<Key>(key_fn(items[i])));
        indices.push_back(i);
    }

    auto key_slice = keys.slice_mut();
    auto idx_slice = indices.slice_mut();

    impl::radix_sort_pairs_inplace(key_slice, idx_slice);

    DynamicArray<Item> reordered;
    reordered.reserve(items.size());

    for (USize i = 0; i < idx_slice.size(); ++i) {
        reordered.push_back(std::move(items[idx_slice[i]]));
    }

    for (USize i = 0; i < reordered.size(); ++i) {
        items[i] = std::move(reordered[i]);
    }
}
} // namespace fr
