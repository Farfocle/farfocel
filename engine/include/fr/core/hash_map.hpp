/**
 * @file hash_map.hpp
 * @author Kiju
 *
 * @brief Dense hash table with linear probing. Utilizes the same design as fr::HashSet.
 */

#pragma once

#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/hash.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/optional.hpp"
#include "fr/core/pair.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
namespace impl {

/**
 * @brief Internal tag for default hash function.
 */
struct HashMapDeafultHashFnTag {};
/**
 * @brief Internal tag for default comparison function.
 */
struct HashMapDeafultEqFnTag {};

/**
 * @brief Default hash function utilizing the call_hash protocol.
 */
template <typename Key>
struct HashMapDeafultHash {
    inline Hash operator()(const Key &key) const noexcept {
        return call_hash(key);
    }
};

/**
 * @brief Default equality comparison utilizing operator==.
 */
template <typename Key>
struct HashMapDeafultEq {
    inline bool operator()(const Key &lhs, const Key &rhs) const noexcept {
        return lhs == rhs;
    }
};
} // namespace impl

/**
 * @brief Dense hash map using linear probing and control-byte filtering.
 * @tparam Key Key type.
 * @tparam Value Value type.
 * @tparam HashFn Hash function type.
 * @tparam CmpFn Equality comparison function type.
 *
 * HashMap provides O(1) average-time complexity for insertions, lookups, and removals.
 * It uses a Swiss Table-like architecture to achieve high cache efficiency.
 */
template <typename Key, typename Value, typename HashFn = impl::HashMapDeafultHashFnTag,
          typename CmpFn = impl::HashMapDeafultEqFnTag>
class HashMap {

    using ActualHashFn = std::conditional_t<std::is_same_v<HashFn, impl::HashMapDeafultHashFnTag>,
                                            impl::HashMapDeafultHash<Key>, HashFn>;

    using ActualCmpFn = std::conditional_t<std::is_same_v<CmpFn, impl::HashMapDeafultEqFnTag>,
                                           impl::HashMapDeafultEq<Key>, CmpFn>;

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

private:
    /**
     * @brief Iterator implementation for HashMap.
     * @tparam IsConst Whether the iterator is constant.
     *
     * Iterators for HashMap are forward iterators. They skip over empty and tombstone slots.
     */
    template <bool IsConst>
    struct IterImpl {
        using iterator_category = std::forward_iterator_tag;
        using KeyRef = const Key &;
        using ValueRef = std::conditional_t<IsConst, const Value &, Value &>;
        using reference = Pair<KeyRef, ValueRef>;
        using value_type = Pair<Key, Value>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;

        using SlotPtr = std::conditional_t<IsConst, const Slot *, Slot *>;
        using CtrlPtr = std::conditional_t<IsConst, const Ctrl *, Ctrl *>;

        /**
         * @brief Dereference operator. Returns a proxy Pair of references.
         */
        reference operator*() const noexcept {
            return reference(m_slot->key, m_slot->value);
        }

        /**
         * @brief Prefix increment. Moves to the next occupied slot.
         */
        IterImpl &operator++() noexcept {
            ++m_ctrl;
            ++m_slot;
            do_skip();
            return *this;
        }

        /**
         * @brief Postfix increment.
         */
        IterImpl operator++(int) noexcept {
            IterImpl copy = *this;
            ++(*this);
            return copy;
        }

        /**
         * @brief Equality comparison for iterators.
         */
        bool operator==(const IterImpl &other) const noexcept {
            return m_ctrl == other.m_ctrl;
        }

        /**
         * @brief Inequality comparison for iterators.
         */
        bool operator!=(const IterImpl &other) const noexcept {
            return m_ctrl != other.m_ctrl;
        }

    private:
        IterImpl(CtrlPtr ctrl, SlotPtr slot) noexcept
            : m_slot(slot),
              m_ctrl(ctrl) {
            do_skip();
        }

        void do_skip() noexcept {
            if (!m_ctrl)
                return;
            while (m_ctrl->is_empty_or_tombstone()) {
                ++m_slot;
                ++m_ctrl;
            }
        }

        SlotPtr m_slot{nullptr};
        CtrlPtr m_ctrl{nullptr};

        friend class HashMap;
    };

public:
    using IterMut = IterImpl<false>;
    using IterConst = IterImpl<true>;

    using iterator = IterMut;
    using const_iterator = IterConst;

    /**
     * @brief Constructs an empty HashMap using the default allocator.
     */
    HashMap() = default;

    /**
     * @brief Constructs an empty HashMap using a specific allocator.
     *
     * @param alloc Pointer to the allocator.
     */
    explicit HashMap(Alloc *alloc) noexcept
        : m_alloc(alloc) {
    }

    /**
     * @brief Copy-constructs a new HashMap.
     * @param other Source map.
     */
    HashMap(const HashMap &other) noexcept
        : m_alloc(other.m_alloc) {
        if (other.m_capacity > 0) {
            do_grow(other.m_capacity);
            for (auto pair : other) {
                auto &[k, v] = pair;
                insert(k, v);
            }
        }
    }

    /**
     * @brief Move-constructs a new HashMap.
     * @param other Source map.
     */
    HashMap(HashMap &&other) noexcept
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
     * @brief Copy-assigns from another HashMap.
     * @param other Source map.
     * @return Reference to this map.
     */
    HashMap &operator=(const HashMap &other) noexcept {
        if (this != &other) {
            clear();
            for (auto pair : other) {
                auto &[k, v] = pair;
                insert(k, v);
            }
        }
        return *this;
    }

    /**
     * @brief Move-assigns from another HashMap.
     * @param other Source map.
     * @return Reference to this map.
     */
    HashMap &operator=(HashMap &&other) noexcept {
        if (this != &other) {
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
        }
        return *this;
    }

    /**
     * @brief Destroys the HashMap.
     */
    ~HashMap() noexcept {
        do_destroy_storage();
    }

    /**
     * @brief Creates an empty HashMap with a specific allocator.
     *
     * @param alloc Pointer to the allocator.
     * @return A new empty HashMap instance.
     * @pre alloc must not be null.
     */
    static HashMap with_allocator(Alloc *alloc) noexcept {
        FR_ASSERT(alloc, "allocator must be non-null");
        return HashMap(alloc);
    }

    /**
     * @brief Returns the number of elements in the map.
     */
    USize load() const noexcept {
        return m_load;
    }

    /**
     * @brief Returns the total number of slots.
     */
    USize capacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief Checks if the map is empty.
     */
    bool is_empty() const noexcept {
        return m_load == 0;
    }

    /**
     * @brief Returns a mutable iterator to the first element.
     */
    IterMut begin() noexcept {
        if (m_capacity == 0)
            return end();
        return IterMut(m_ctrls, m_slots);
    }

    /**
     * @brief Returns a mutable iterator to the end.
     */
    IterMut end() noexcept {
        if (m_capacity == 0)
            return IterMut(nullptr, nullptr);
        return IterMut(m_ctrls + m_capacity, m_slots + m_capacity);
    }

    /**
     * @brief Returns a constant iterator to the first element.
     */
    IterConst begin() const noexcept {
        if (m_capacity == 0)
            return end();
        return IterConst(m_ctrls, m_slots);
    }

    /**
     * @brief Returns a constant iterator to the end.
     */
    IterConst end() const noexcept {
        if (m_capacity == 0)
            return IterConst(nullptr, nullptr);
        return IterConst(m_ctrls + m_capacity, m_slots + m_capacity);
    }

    /**
     * @brief Constant iterator entry point.
     */
    IterConst cbegin() const noexcept {
        return begin();
    }

    /**
     * @brief Constant iterator end point.
     */
    IterConst cend() const noexcept {
        return end();
    }

    /**
     * @brief Returns the maximum number of elements before rehash.
     */
    USize max_load() const noexcept {
        return (m_capacity * 7) / 8;
    }

    /**
     * @brief Checks if a key exists in the map.
     * @param key Key to find.
     * @return True if key exists.
     */
    bool contains(const Key &key) const noexcept {
        return do_find_idx(key) != -1;
    }

    /**
     * @brief Returns an Optional pointer to the value.
     * @param key Key to find.
     * @return Optional containing pointer to value if found.
     */
    Optional<Value *> find(const Key &key) noexcept {
        std::ptrdiff_t idx = do_find_idx(key);
        return idx == -1 ? none() : some(&m_slots[idx].value);
    }

    /**
     * @brief Returns a constant Optional pointer to the value.
     * @param key Key to find.
     * @return Optional containing constant pointer to value if found.
     */
    Optional<const Value *> find(const Key &key) const noexcept {
        std::ptrdiff_t idx = do_find_idx(key);
        return idx == -1 ? none() : some(static_cast<const Value *>(&m_slots[idx].value));
    }

    /**
     * @brief Inserts a key-value pair.
     * @param key Key to insert.
     * @param value Value to insert.
     * @return True if insertion happened, false if key already exists.
     */
    bool insert(const Key &key, const Value &value) {
        return do_insert(key, value);
    }

    /**
     * @brief Inserts a key-value pair by moving the value.
     * @param key Key to insert.
     * @param value Value to move.
     * @return True if insertion happened, false if key already exists.
     */
    bool insert(const Key &key, Value &&value) {
        return do_insert(key, std::move(value));
    }

    /**
     * @brief Constructs a value in-place for the given key.
     * @tparam Args Argument types.
     * @param key Key to insert.
     * @param args Arguments for Value constructor.
     * @return True if insertion happened, false if key already exists.
     */
    template <typename... Args>
    bool emplace(const Key &key, Args &&...args) {
        return do_insert(key, Value(std::forward<Args>(args)...));
    }

    /**
     * @brief Removes an entry.
     * @param key Key to remove.
     * @return True if entry was found and removed.
     */
    bool remove(const Key &key) noexcept {
        std::ptrdiff_t idx = do_find_idx(key);
        if (idx == -1)
            return false;

        m_ctrls[idx].value = Ctrl::tombstone;
        std::destroy_at(&m_slots[idx].key);
        std::destroy_at(&m_slots[idx].value);
        --m_load;

        return true;
    }

    /**
     * @brief Removes all elements.
     */
    void clear() noexcept {
        if (!m_slots)
            return;
        for (USize i = 0; i < m_capacity; ++i) {
            if (m_ctrls[i].is_occupied()) {
                std::destroy_at(&m_slots[i].key);
                std::destroy_at(&m_slots[i].value);
            }
            m_ctrls[i].value = Ctrl::empty;
        }

        m_load = 0;
    }

    /**
     * @brief Access-or-insert lookup.
     *
     * 1. If the key exists, it returns a reference to the existing value.
     * 2. If the key is missing, it default-constructs a new Value, inserts it,
     *    and returns a reference to it.
     *
     * @param key Key to look up.
     * @return Reference to the value.
     * @note May trigger a rehash/growth.
     */
    Value &operator[](const Key &key) {
        std::ptrdiff_t idx = do_find_idx(key);
        if (idx != -1) {
            return m_slots[idx].value;
        }

        if (m_capacity == 0 || m_load + 1 > max_load()) {
            do_grow(m_capacity == 0 ? 16 : m_capacity * 2);
        }

        Hash h = ActualHashFn{}(key);
        USize mask = m_capacity - 1;
        USize start_idx = h.h57() & mask;
        S8 h2 = static_cast<S8>(h.l7());

        std::ptrdiff_t target_idx = -1;
        for (USize i = 0; i < m_capacity; ++i) {
            USize curr = (start_idx + i) & mask;
            Ctrl c = m_ctrls[curr];
            if (c.value == Ctrl::empty || c.value == Ctrl::tombstone) {
                target_idx = static_cast<std::ptrdiff_t>(curr);
                break;
            }
        }

        FR_ASSERT(target_idx != -1, "hash map overflow");
        USize target = static_cast<USize>(target_idx);

        m_ctrls[target].value = h2;
        m_slots[target].hash = h;
        std::construct_at(&m_slots[target].key, key);
        std::construct_at(&m_slots[target].value);
        ++m_load;

        return m_slots[target].value;
    }

private:
    std::ptrdiff_t do_find_idx(const Key &key) const noexcept {
        if (m_capacity == 0)
            return -1;

        Hash h = ActualHashFn{}(key);
        USize mask = m_capacity - 1;
        USize idx = h.h57() & mask;
        S8 h2 = static_cast<S8>(h.l7());

        for (USize i = 0; i < m_capacity; ++i) {
            USize curr = (idx + i) & mask;
            Ctrl c = m_ctrls[curr];

            if (c.value == Ctrl::empty)
                return -1;

            if (c.value == h2) {
                if (ActualCmpFn{}(m_slots[curr].key, key)) {
                    return static_cast<std::ptrdiff_t>(curr);
                }
            }
        }

        return -1;
    }

    template <typename K, typename V>
    bool do_insert(K &&key, V &&value) {
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

        FR_ASSERT(first_free != -1, "hash map overflow");
        USize target = static_cast<USize>(first_free);

        m_ctrls[target].value = h2;
        m_slots[target].hash = h;
        std::construct_at(&m_slots[target].key, std::forward<K>(key));
        std::construct_at(&m_slots[target].value, std::forward<V>(value));
        ++m_load;

        return true;
    }

    void do_grow(USize new_capacity) noexcept {
        FR_ASSERT(new_capacity > m_capacity, "capacity must grow");
        FR_ASSERT(math::is_pow2(new_capacity), "capacity must be power of two");

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

        for (USize i = 0; i < new_capacity; ++i) {
            m_ctrls[i].value = Ctrl::empty;
        }

        for (USize i = 0; i < 16; ++i) {
            m_ctrls[new_capacity + i].value = Ctrl::sentinel;
        }

        if (!old_slots) {
            return;
        }

        for (USize i = 0; i < old_capacity; ++i) {
            if (old_ctrls[i].is_occupied()) {
                do_rehash_insert(std::move(old_slots[i].key), std::move(old_slots[i].value),
                                 old_slots[i].hash);
                std::destroy_at(&old_slots[i].key);
                std::destroy_at(&old_slots[i].value);
            }
        }

        m_alloc->deallocate(old_slots, do_sizeof_storage(old_capacity), alignment);
    }

    void do_rehash_insert(Key &&key, Value &&value, Hash h) noexcept {
        USize mask = m_capacity - 1;
        USize idx = h.h57() & mask;

        while (m_ctrls[idx].is_occupied()) {
            idx = (idx + 1) & mask;
        }

        m_ctrls[idx].value = static_cast<S8>(h.l7());
        m_slots[idx].hash = h;

        std::construct_at(&m_slots[idx].key, std::move(key));
        std::construct_at(&m_slots[idx].value, std::move(value));
        ++m_load;
    }

    void do_destroy_storage() noexcept {
        if (!m_slots) {
            return;
        }

        for (USize i = 0; i < m_capacity; ++i) {
            if (m_ctrls[i].is_occupied()) {
                std::destroy_at(&m_slots[i].key);
                std::destroy_at(&m_slots[i].value);
            }
        }

        m_alloc->deallocate(m_slots, do_sizeof_storage(m_capacity), alignof(Slot));

        m_capacity = 0;
        m_load = 0;
        m_ctrls = nullptr;
        m_slots = nullptr;
    }

    USize do_sizeof_storage(USize capacity) const noexcept {
        return do_sizeof_ctrls(capacity) + do_sizeof_slots(capacity);
    }

    USize do_sizeof_ctrls(USize capacity) const noexcept {
        return sizeof(Ctrl) * (capacity + 16);
    }

    USize do_sizeof_slots(USize capacity) const noexcept {
        return sizeof(Slot) * capacity;
    }

    Alloc *m_alloc{globals::get_default_allocator()};
    USize m_capacity{0};
    USize m_load{0};
    Slot *m_slots{nullptr};
    Ctrl *m_ctrls{nullptr};
};

} // namespace fr
