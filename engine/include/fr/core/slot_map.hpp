/**
 * @file slot_map.hpp
 * @author Tfoedy
 *
 * @brief Generational slot map storage.
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

constexpr USize DEF_SLOTMAP_ALLOC_COUNT = 128;
constexpr U32 INVALID_SLOT_INDEX = 0xFFFFFFFFu;

struct SlotKey {
    U32 index{0};

    /// @brief Even generation means dead/free, odd generation means alive.
    U32 generation{0};

    [[nodiscard]] bool is_valid_generation() const noexcept {
        return (generation & 1u) != 0;
    }

    [[nodiscard]] bool operator==(const SlotKey &other) const noexcept {
        return index == other.index && generation == other.generation;
    }

    [[nodiscard]] bool operator!=(const SlotKey &other) const noexcept {
        return !(*this == other);
    }
};

template <typename T>
struct Slot {
    U32 generation{0};

    union {
        U32 next_free_index = INVALID_SLOT_INDEX;
        alignas(T) unsigned char data[sizeof(T)];
    };
};

template <typename T>
class SlotMap {
    static_assert(std::is_nothrow_move_constructible_v<T>,
                  "SlotMap<T> requires nothrow move construction");
    static_assert(std::is_nothrow_destructible_v<T>, "SlotMap<T> requires nothrow destruction");

public:
    SlotMap()
        : SlotMap(static_cast<U32>(DEF_SLOTMAP_ALLOC_COUNT)) {
    }

    explicit SlotMap(U32 preallocation_count) {
        FR_ASSERT(preallocation_count > 0, "preallocation count must be greater than 0");
        reserve(preallocation_count);
    }

    SlotMap(const SlotMap &) = delete;
    SlotMap &operator=(const SlotMap &) = delete;

    SlotMap(SlotMap &&other) noexcept
        : m_slots(std::move(other.m_slots)),
          m_next_free_index(other.m_next_free_index),
          m_size(other.m_size) {
        other.m_next_free_index = INVALID_SLOT_INDEX;
        other.m_size = 0;
    }

    SlotMap &operator=(SlotMap &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        clear();

        m_slots = std::move(other.m_slots);
        m_next_free_index = other.m_next_free_index;
        m_size = other.m_size;

        other.m_next_free_index = INVALID_SLOT_INDEX;
        other.m_size = 0;

        return *this;
    }

    ~SlotMap() noexcept {
        clear();
    }

    template <typename... Args>
    [[nodiscard]] SlotKey add(Args &&...args) {
        if (m_next_free_index == INVALID_SLOT_INDEX) {
            const U32 current_capacity = static_cast<U32>(m_slots.size());
            const U32 new_capacity = current_capacity > 0
                                         ? current_capacity * 2
                                         : static_cast<U32>(DEF_SLOTMAP_ALLOC_COUNT);

            reserve(new_capacity);
        }

        const U32 index = m_next_free_index;
        Slot<T> &slot = m_slots[index];

        FR_ASSERT(!do_is_alive(slot.generation), "free slot has an alive generation");

        m_next_free_index = slot.next_free_index;

        ++slot.generation;

        if (slot.generation == 0) [[unlikely]] {
            ++slot.generation;
        }

        new (static_cast<void *>(slot.data)) T(std::forward<Args>(args)...);

        ++m_size;
        return SlotKey{index, slot.generation};
    }

    [[nodiscard]] T *get_data(SlotKey key) noexcept {
        if (key.index >= m_slots.size()) {
            return nullptr;
        }

        Slot<T> &slot = m_slots[key.index];

        if (slot.generation != key.generation) {
            return nullptr;
        }

        if (!do_is_alive(key.generation)) {
            return nullptr;
        }

        return do_data_ptr(slot);
    }

    [[nodiscard]] const T *get_data(SlotKey key) const noexcept {
        if (key.index >= m_slots.size()) {
            return nullptr;
        }

        const Slot<T> &slot = m_slots[key.index];

        if (slot.generation != key.generation) {
            return nullptr;
        }

        if (!do_is_alive(key.generation)) {
            return nullptr;
        }

        return do_data_ptr(slot);
    }

    /**
     * @brief Returns slot data without validating the generation.
     * @pre key.index must be in range and the slot must be alive.
     */
    [[nodiscard]] T *get_data_unsafe(SlotKey key) noexcept {
        FR_ASSERT(key.index < m_slots.size(), "slot index outside range");
        FR_ASSERT(do_is_alive(m_slots[key.index].generation), "slot is not alive");

        return do_data_ptr(m_slots[key.index]);
    }

    /**
     * @brief Returns slot data without validating the generation.
     * @pre key.index must be in range and the slot must be alive.
     */
    [[nodiscard]] const T *get_data_unsafe(SlotKey key) const noexcept {
        FR_ASSERT(key.index < m_slots.size(), "slot index outside range");
        FR_ASSERT(do_is_alive(m_slots[key.index].generation), "slot is not alive");

        return do_data_ptr(m_slots[key.index]);
    }

    bool erase(SlotKey key) noexcept {
        T *obj = get_data(key);
        if (!obj) {
            return false;
        }

        obj->~T();

        Slot<T> &slot = m_slots[key.index];

        ++slot.generation;
        slot.next_free_index = m_next_free_index;
        m_next_free_index = key.index;

        FR_ASSERT(m_size > 0, "SlotMap size underflow");
        --m_size;

        return true;
    }

    [[nodiscard]] USize get_size() const noexcept {
        return m_size;
    }

    [[nodiscard]] USize get_capacity() const noexcept {
        return m_slots.size();
    }

    [[nodiscard]] bool is_empty() const noexcept {
        return m_size == 0;
    }

    template <typename Fn>
    void for_each_alive(Fn &&fn) noexcept {
        const U32 capacity = static_cast<U32>(m_slots.size());

        for (U32 i = 0; i < capacity; ++i) {
            Slot<T> &slot = m_slots[i];

            if (!do_is_alive(slot.generation)) {
                continue;
            }

            fn(SlotKey{i, slot.generation}, *do_data_ptr(slot));
        }
    }

    template <typename Fn>
    void for_each_alive(Fn &&fn) const noexcept {
        const U32 capacity = static_cast<U32>(m_slots.size());

        for (U32 i = 0; i < capacity; ++i) {
            const Slot<T> &slot = m_slots[i];

            if (!do_is_alive(slot.generation)) {
                continue;
            }

            fn(SlotKey{i, slot.generation}, *do_data_ptr(slot));
        }
    }

    void clear() noexcept {
        const U32 capacity = static_cast<U32>(m_slots.size());

        if (capacity == 0) {
            m_next_free_index = INVALID_SLOT_INDEX;
            m_size = 0;
            return;
        }

        for (U32 i = 0; i < capacity; ++i) {
            Slot<T> &slot = m_slots[i];

            if (do_is_alive(slot.generation)) {
                do_data_ptr(slot)->~T();
                ++slot.generation;
            }

            slot.generation &= ~1u;
            slot.next_free_index = i + 1;
        }

        m_slots[capacity - 1].next_free_index = INVALID_SLOT_INDEX;

        m_next_free_index = 0;
        m_size = 0;
    }

    /**
     * @brief Reserves at least capacity slots.
     *
     * @details Active objects are moved with their move constructors. This keeps SlotMap valid for
     * non-trivial types such as DynamicArray/String owning asset records.
     */
    void reserve(U32 capacity) {
        const U32 old_capacity = static_cast<U32>(m_slots.size());

        if (capacity <= old_capacity) {
            return;
        }

        DynamicArray<Slot<T>> new_slots;
        new_slots.reserve(capacity);

        for (U32 i = 0; i < capacity; ++i) {
            new_slots.emplace_back();
        }

        for (U32 i = 0; i < old_capacity; ++i) {
            Slot<T> &src = m_slots[i];
            Slot<T> &dst = new_slots[i];

            dst.generation = src.generation;

            if (do_is_alive(src.generation)) {
                new (static_cast<void *>(dst.data)) T(std::move(*do_data_ptr(src)));
                do_data_ptr(src)->~T();
            } else {
                dst.next_free_index = src.next_free_index;
            }
        }

        for (U32 i = old_capacity; i < capacity; ++i) {
            Slot<T> &slot = new_slots[i];

            slot.generation = 0;
            slot.next_free_index = (i + 1 < capacity) ? i + 1 : m_next_free_index;
        }

        m_slots = std::move(new_slots);
        m_next_free_index = old_capacity;
    }

private:
    [[nodiscard]] static bool do_is_alive(U32 generation) noexcept {
        return (generation & 1u) != 0;
    }

    [[nodiscard]] static T *do_data_ptr(Slot<T> &slot) noexcept {
        return reinterpret_cast<T *>(slot.data);
    }

    [[nodiscard]] static const T *do_data_ptr(const Slot<T> &slot) noexcept {
        return reinterpret_cast<const T *>(slot.data);
    }

private:
    DynamicArray<Slot<T>> m_slots{};
    U32 m_next_free_index{INVALID_SLOT_INDEX};
    USize m_size{0};
};

} // namespace fr
