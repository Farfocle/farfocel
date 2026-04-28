/**
 * @file hash_set.hpp
 * @author Kiju
 *
 * @brief Dense hash set with linear probing utilizing swiss table architecture.
 */

#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

#include "fr/core/allocator.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/hash.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

namespace impl_hash_set {
/**
 * @brief Internal tag for default hash function.
 */
struct DeafultHashFnTag {};
/**
 * @brief Internal tag for default comparison function.
 */
struct DeafultCmpFnTag {};

/**
 * @brief Default hash function utilizing the call_hash protocol.
 */
template <typename Key>
struct DefaultHash {
    inline Hash operator()(const Key &key) const noexcept {
        return call_hash(key);
    }
};

/**
 * @brief Default equality comparison utilizing operator==.
 */
template <typename Key>
struct DefaultCmp {
    inline bool operator()(const Key &lhs, const Key &rhs) const noexcept {
        return lhs == rhs;
    }
};

} // namespace impl_hash_set

/**
 * @brief Swiss-table inspired hash set.
 * @tparam Key Element type.
 * @tparam HashFn Hash function type.
 * @tparam CmpFn Equality comparison function type.
 *
 * This implementation uses a flat memory layout with control bytes (Swiss Table)
 * to speed up lookups and minimize cache misses.
 */
template <typename Key, typename HashFn = impl_hash_set::DeafultHashFnTag,
          typename CmpFn = impl_hash_set::DeafultCmpFnTag>
class HashSet {
    using ActualHashFn = std::conditional_t<std::is_same_v<HashFn, impl_hash_set::DeafultHashFnTag>,
                                            impl_hash_set::DefaultHash<Key>, HashFn>;
    using ActualCmpFn = std::conditional_t<std::is_same_v<CmpFn, impl_hash_set::DeafultCmpFnTag>,
                                           impl_hash_set::DefaultCmp<Key>, CmpFn>;

private:
    struct Slot {
        Hash hash;
        Key key;
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

public:
    /**
     * @brief Iterator for the HashSet. Skips over empty and tombstone slots.
     */
    struct Iter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Key;
        using difference_type = std::ptrdiff_t;
        using pointer = const Key *;
        using reference = const Key &;

        /**
         * @brief Access the key.
         * @return Constant reference to the key.
         */
        const Key &operator*() const noexcept {
            return m_slot->key;
        }

        /**
         * @brief Access the key via pointer.
         * @return Constant pointer to the key.
         */
        const Key *operator->() const noexcept {
            return &m_slot->key;
        }

        /**
         * @brief Prefix increment. Moves to the next occupied slot.
         * @return Reference to this iterator.
         */
        Iter &operator++() noexcept {
            ++m_ctrl;
            ++m_slot;
            do_skip();
            return *this;
        }

        /**
         * @brief Postfix increment.
         * @return Copy of the iterator before increment.
         */
        Iter operator++(int) {
            Iter copy = *this;
            ++(*this);
            return copy;
        }

        /**
         * @brief Equality comparison for iterators.
         */
        bool operator==(const Iter &other) const noexcept {
            return m_ctrl == other.m_ctrl;
        }

        /**
         * @brief Inequality comparison for iterators.
         */
        bool operator!=(const Iter &other) const noexcept {
            return m_ctrl != other.m_ctrl;
        }

    private:
        Iter(Ctrl *ctrl, Slot *slot) noexcept
            : m_slot(slot),
              m_ctrl(ctrl) {
            do_skip();
        }

        void do_skip() noexcept {
            // Skips non-occupied slots until an occupied one or the sentinel is found.
            while (m_ctrl->is_empty_or_tombstone()) {
                ++m_slot;
                ++m_ctrl;
            }
        }

        Slot *m_slot{nullptr};
        Ctrl *m_ctrl{nullptr};

        friend class HashSet;
    };

public:
    using iterator = Iter;
    using const_iterator = Iter;

    /**
     * @brief Constructs an empty HashSet using the default allocator.
     */
    HashSet() = default;

    /**
     * @brief Constructs an empty HashSet using a specific allocator.
     * @param alloc Pointer to the allocator to use.
     */
    explicit HashSet(Allocator *alloc) noexcept
        : m_alloc(alloc) {
    }

    /**
     * @brief Copy-constructs a new HashSet.
     * @param other The HashSet to copy from.
     * @note Performs a deep copy of all elements.
     */
    HashSet(const HashSet &other) noexcept
        : m_alloc(other.m_alloc) {
        if (other.m_capacity == 0) {
            return;
        }

        do_grow(other.m_capacity);
        for (const auto &key : other) {
            insert(key);
        }
    }

    /**
     * @brief Move-constructs a new HashSet, stealing storage from @p other.
     * @param other The HashSet to move from.
     */
    HashSet(HashSet &&other) noexcept
        : m_alloc(other.m_alloc),
          m_capacity(other.m_capacity),
          m_load(other.m_load),
          m_slots(other.m_slots),
          m_ctrls(other.m_ctrls) {
        other.m_slots = nullptr;
        other.m_ctrls = nullptr;
        other.m_capacity = 0;
        other.m_load = 0;
    }

    /**
     * @brief Copy-assigns from another HashSet.
     * @param other The HashSet to copy from.
     * @return Reference to this set.
     */
    HashSet &operator=(const HashSet &other) noexcept {
        if (this == &other) {
            return *this;
        }

        clear();

        for (const auto &key : other) {
            insert(key);
        }

        return *this;
    }

    /**
     * @brief Move-assigns from another HashSet.
     * @param other The HashSet to move from.
     * @return Reference to this set.
     */
    HashSet &operator=(HashSet &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        do_destroy_storage();
        m_alloc = other.m_alloc;
        m_capacity = other.m_capacity;
        m_load = other.m_load;
        m_slots = other.m_slots;
        m_ctrls = other.m_ctrls;

        other.m_slots = nullptr;
        other.m_ctrls = nullptr;
        other.m_capacity = 0;
        other.m_load = 0;

        return *this;
    }

    /**
     * @brief Destroys the HashSet and all its elements.
     */
    ~HashSet() noexcept {
        do_destroy_storage();
    }

    /**
     * @brief Creates an empty HashSet with a specific allocator.
     * @param alloc Pointer to the allocator.
     * @return A new empty HashSet instance.
     * @pre @p alloc must not be null.
     */
    static HashSet with_allocator(Allocator *alloc) noexcept {
        FR_ASSERT(alloc, "Allocator must not be null");
        return HashSet(alloc);
    }

    /**
     * @brief Returns the number of elements currently in the set.
     */
    USize load() const noexcept {
        return m_load;
    }

    /**
     * @brief Returns the total number of slots available in the current storage.
     */
    USize capacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief Checks if the set is empty.
     */
    bool is_empty() const noexcept {
        return m_load == 0;
    }

    /**
     * @brief Returns an iterator to the first element.
     */
    Iter begin() const noexcept {
        if (m_capacity == 0) {
            return end();
        }

        return Iter(m_ctrls, m_slots);
    }

    /**
     * @brief Returns an iterator to the element following the last element.
     */
    Iter end() const noexcept {
        if (m_capacity == 0) {
            return Iter(nullptr, nullptr);
        }

        return Iter(m_ctrls + m_capacity, m_slots + m_capacity);
    }

    /**
     * @brief Constant iterator entry point.
     */
    Iter cbegin() const noexcept {
        return begin();
    }

    /**
     * @brief Constant iterator end point.
     */
    Iter cend() const noexcept {
        return end();
    }

    /**
     * @brief Returns the maximum number of elements before a rehash is triggered.
     */
    USize max_load() const noexcept {
        return (m_capacity * 7) / 8;
    }

    /**
     * @brief Checks if a key exists in the set.
     * @param key The key to search for.
     * @return True if the key is present.
     */
    bool contains(const Key &key) const noexcept {
        return do_find_idx(key) != -1;
    }

    /**
     * @brief Inserts a copy of the key.
     * @param key The key to insert.
     * @return True if the key was inserted, false if it already exists.
     * @note May trigger a rehash/growth.
     */
    bool insert(const Key &key) {
        return do_insert(key);
    }

    /**
     * @brief Inserts a key by moving it.
     * @param key The key to move.
     * @return True if the key was inserted, false if it already exists.
     * @note May trigger a rehash/growth.
     */
    bool insert(Key &&key) {
        return do_insert(std::move(key));
    }

    /**
     * @brief Constructs a key in-place.
     * @tparam Args Constructor argument types.
     * @param args Arguments for the key constructor.
     * @return True if the key was inserted, false if it already exists.
     * @note May trigger a rehash/growth.
     */
    template <typename... Args>
    bool emplace(Args &&...args) {
        return do_insert(Key(std::forward<Args>(args)...));
    }

    /**
     * @brief Removes a key.
     * @param key The key to remove.
     * @return True if the key was found and removed, false otherwise.
     * @note Leaves a tombstone in the removed slot.
     */
    bool remove(const Key &key) noexcept {
        std::ptrdiff_t idx = do_find_idx(key);
        if (idx == -1) {
            return false;
        }

        m_ctrls[idx].value = Ctrl::tombstone;
        std::destroy_at(&m_slots[idx].key);
        --m_load;

        return true;
    }

    /**
     * @brief Removes all elements without deallocating storage.
     */
    void clear() noexcept {
        for (USize i = 0; i < m_capacity; ++i) {
            if (m_ctrls[i].is_occupied()) {
                std::destroy_at(&m_slots[i].key);
            }
            m_ctrls[i].value = Ctrl::empty;
        }

        m_load = 0;
    }

private:
    std::ptrdiff_t do_find_idx(const Key &key) const noexcept {
        if (m_capacity == 0) {
            return -1;
        }

        Hash h = ActualHashFn{}(key);
        USize mask = m_capacity - 1;
        USize idx = h.h57() & mask;
        S8 h2 = static_cast<S8>(h.l7());

        for (USize i = 0; i < m_capacity; ++i) {
            USize curr = (idx + i) & mask;
            Ctrl c = m_ctrls[curr];

            if (c.value == Ctrl::empty) {
                return -1;
            }

            if (c.value == h2) {
                if (ActualCmpFn{}(m_slots[curr].key, key)) {
                    return static_cast<std::ptrdiff_t>(curr);
                }
            }
        }

        return -1;
    }

    template <typename K>
    bool do_insert(K &&key) {
        if (m_capacity == 0 || m_load + 1 > max_load()) {
            do_grow(m_capacity == 0 ? 16 : m_capacity * 2);
        }

        Hash h = ActualHashFn{}(key);
        USize mask = m_capacity - 1;
        USize idx = h.h57() & mask;
        S8 h2 = static_cast<S8>(h.l7());

        std::ptrdiff_t first_free = -1;

        for (USize i = 0; i < m_capacity; ++i) {
            USize curr = (idx + i) & mask;
            Ctrl c = m_ctrls[curr];

            if (c.value == h2) {
                if (ActualCmpFn{}(m_slots[curr].key, key)) {
                    return false;
                }
            }

            if (c.value == Ctrl::empty) {
                if (first_free == -1) {
                    first_free = static_cast<std::ptrdiff_t>(curr);
                }

                break;
            }

            if (c.value == Ctrl::tombstone && first_free == -1) {
                first_free = static_cast<std::ptrdiff_t>(curr);
            }
        }

        FR_ASSERT(first_free != -1, "HashSet: Failed to find insertion slot");
        USize target = static_cast<USize>(first_free);

        m_ctrls[target].value = h2;
        m_slots[target].hash = h;
        std::construct_at(&m_slots[target].key, std::forward<K>(key));
        ++m_load;

        return true;
    }

    void do_grow(USize new_capacity) noexcept {
        FR_ASSERT(new_capacity > m_capacity, "New capacity has to be greater than the old one");
        FR_ASSERT(math::is_pow2(new_capacity), "Capacity must be a power of two");

        USize old_capacity = m_capacity;
        Slot *old_slots = m_slots;
        Ctrl *old_ctrls = m_ctrls;

        m_capacity = new_capacity;
        m_load = 0;

        USize slots_size = do_sizeof_slots(new_capacity);
        USize ctrls_size = do_sizeof_ctrls(new_capacity);
        USize alignment = alignof(Slot);

        Byte *buffer =
            reinterpret_cast<Byte *>(m_alloc->allocate(slots_size + ctrls_size, alignment));
        m_slots = reinterpret_cast<Slot *>(buffer);
        m_ctrls = reinterpret_cast<Ctrl *>(buffer + slots_size);

        // Initialize ctrls with empty, and trailing bytes with sentinel.
        for (USize i = 0; i < new_capacity; ++i) {
            m_ctrls[i].value = Ctrl::empty;
        }

        for (USize i = 0; i < 16; ++i) {
            m_ctrls[new_capacity + i].value = Ctrl::sentinel;
        }

        if (old_slots) {
            for (USize i = 0; i < old_capacity; ++i) {
                if (old_ctrls[i].is_occupied()) {
                    do_rehash_insert(std::move(old_slots[i].key), old_slots[i].hash);
                    std::destroy_at(&old_slots[i].key);
                }
            }

            m_alloc->deallocate(old_slots,
                                do_sizeof_slots(old_capacity) + do_sizeof_ctrls(old_capacity),
                                alignment);
        }
    }

    void do_rehash_insert(Key &&key, Hash h) noexcept {
        USize mask = m_capacity - 1;
        USize idx = h.h57() & mask;

        while (m_ctrls[idx].is_occupied()) {
            idx = (idx + 1) & mask;
        }

        m_ctrls[idx].value = static_cast<S8>(h.l7());
        m_slots[idx].hash = h;
        std::construct_at(&m_slots[idx].key, std::move(key));
        ++m_load;
    }

    void do_destroy_storage() noexcept {
        if (m_slots) {
            for (USize i = 0; i < m_capacity; ++i) {
                if (m_ctrls[i].is_occupied()) {
                    std::destroy_at(&m_slots[i].key);
                }
            }

            m_alloc->deallocate(m_slots, do_sizeof_slots(m_capacity) + do_sizeof_ctrls(m_capacity),
                                alignof(Slot));
            m_slots = nullptr;
            m_ctrls = nullptr;
            m_capacity = 0;
            m_load = 0;
        }
    }

    USize do_sizeof_ctrls(USize capacity) const noexcept {
        return sizeof(Ctrl) * (capacity + 16);
    }

    USize do_sizeof_slots(USize capacity) const noexcept {
        return sizeof(Slot) * capacity;
    }

    Allocator *m_alloc{globals::get_default_allocator()};

    USize m_capacity{0};
    USize m_load{0};
    Slot *m_slots{nullptr};
    Ctrl *m_ctrls{nullptr};
};

} // namespace fr
