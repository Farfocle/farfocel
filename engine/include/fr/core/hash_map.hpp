/**
 * @file hash_map.hpp
 * @author Kiju
 *
 * @brief Dense hash table with linear probing. Utilizes the same design as fr::HashSet.
 */

#pragma once

#include <iterator>

#include "fr/core/hash.hpp"

namespace fr {
namespace impl_hash_map {

struct DeafultHashFnTag {};
struct DeafultCmpFnTag {};

template <typename Key>
struct DefaultHash {
    inline Hash operator()(const Key &key) const noexcept {
        return call_hash(key);
    }
};

template <typename Key>
struct DefaultCmp {
    inline bool operator()(const Key &lhs, const Key &rhs) const noexcept {
        return lhs == rhs;
    }
};
} // namespace impl_hash_map

template <typename Key, typename Value, typename HashFn = impl_hash_map::DeafultHashFnTag,
          typename CmpFn = impl_hash_map::DeafultCmpFnTag>
class HashMap {

    using ActualHashFn = std::conditional_t<std::is_same_v<HashFn, impl_hash_map::DeafultHashFnTag>,
                                            impl_hash_map::DefaultHash<Key>, HashFn>;
    using ActualCmpFn = std::conditional_t<std::is_same_v<CmpFn, impl_hash_map::DeafultCmpFnTag>,
                                           impl_hash_map::DefaultCmp<Key>, CmpFn>;

private:
    struct Slot {
        Hash hash;
        Key key;
        Value value;
    };

    struct Ctrl {
        static constexpr S8 empty = -128;
        static constexpr S8 tombstone = -2;
        static constexpr S8 sentinel = -1;

        S8 value{empty};

        inline bool is_occupied() const noexcept {
            return value >= 0;
        }

        inline bool is_empty_or_tombstone() const noexcept {
            return value < -1;
        }
    };

    struct Iter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Key;
        using difference_type = std::ptrdiff_t;
        using pointer = const Key *;
        using reference = const Key &;
    };
};
} // namespace fr
