/**
 * @file slot_map.hpp
 * @author Tfoedy
 *
 * @brief Farfocel's SlotMap!
 *
 *
 */

#pragma once
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"
#include <type_traits>

namespace fr {
constexpr USize DEF_SLOTMAP_ALLOC_COUNT = 128;
struct SlotKey {
    U32 index = 0;
    // even number of generation = not alive, odd = alive and used
    U32 generation = 0;

    [[nodiscard]] bool operator==(const SlotKey &other) const noexcept {
        return index == other.index && generation == other.generation;
    }
    [[nodiscard]] bool operator!=(const SlotKey &other) const noexcept {
        return !(*this == other);
    }
};

template <typename T>
struct Slot {
    U32 generation = 0;
    union {
        U32 next_free_index = 0xFFFFFFFF;
        alignas(T) unsigned char data[sizeof(T)];
    };
};

template <typename T>
class SlotMap {
public:
    SlotMap()
        : SlotMap(DEF_SLOTMAP_ALLOC_COUNT) {
    }
    explicit SlotMap(U32 preallocation_count) {
        FR_ASSERT(preallocation_count > 0, "prealloc count must be greated than 0");
        reserve(preallocation_count);
    }

    SlotMap(const SlotMap &) = delete;
    SlotMap &operator=(const SlotMap &) = delete;

    SlotMap(SlotMap &&) noexcept = default;
    SlotMap &operator=(SlotMap &&) noexcept = default;

    ~SlotMap() {
        clear();
    }

    template <typename... Args>
    [[nodiscard]] SlotKey add(Args &&...args) {
        if (m_next_free_index == 0xFFFFFFFF) {
            U32 current_capacity = m_slots.size();
            // geometrical growth
            U32 new_capacity =
                (current_capacity > 0) ? (current_capacity * 2) : DEF_SLOTMAP_ALLOC_COUNT;
            reserve(new_capacity);
        }

        U32 index = m_next_free_index;
        m_next_free_index = m_slots[index].next_free_index;

        m_slots[index].generation++;

        new (&m_slots[index].data) T(std::forward<Args>(args)...);
        m_size++;
        return SlotKey{index, m_slots[index].generation};
    }

    [[nodiscard]] T *get_data(SlotKey key) noexcept {
        if (key.index >= m_slots.size())
            return nullptr;

        if (m_slots[key.index].generation != key.generation)
            return nullptr;

        return reinterpret_cast<T *>(m_slots[key.index].data);
    }

    [[nodiscard]] T *get_data_unsafe(SlotKey key) noexcept {
        FR_ASSERT(key.index < m_slots.size(), "index outside range");
        return reinterpret_cast<T *>(m_slots[key.index].data);
    }

    [[nodiscard]] const T *get_data(SlotKey key) const noexcept {
        if (key.index >= m_slots.size())
            return nullptr;

        if (m_slots[key.index].generation != key.generation)
            return nullptr;

        return reinterpret_cast<const T *>(m_slots[key.index].data);
    }

    bool erase(SlotKey key) noexcept {
        T *obj = get_data(key);
        if (!obj)
            return false;

        obj->~T();

        auto &slot = m_slots[key.index];
        slot.generation++;

        slot.next_free_index = m_next_free_index;
        m_next_free_index = key.index;

        m_size--;

        return true;
    }

    [[nodiscard]] USize get_size() const noexcept {
        return m_size;
    }

    [[nodiscard]] USize get_capacity() const noexcept {
        return m_slots.size();
    }

    void clear() noexcept {
        if (m_size == 0)
            return;

        FR_ASSERT(m_slots.size() > 0, "clear called on an empty buffer");

        const U32 capacity = static_cast<U32>(m_slots.size());

        // little optimization for basic data types with no allocations
        if constexpr (std::is_trivially_destructible_v<T>) {
            for (U32 i = 0; i < capacity; ++i) {
                auto &slot = m_slots[i];
                // equivalent to: if (gen % 2 != 0) gen++; but without branching
                slot.generation = (slot.generation + 1) & ~1u;
                slot.next_free_index = i + 1;
            }
        } else {
            for (U32 i = 0; i < capacity; i++) {
                auto &slot = m_slots[i];
                // if used
                if (slot.generation % 2 != 0) {
                    reinterpret_cast<T *>(&slot.data)->~T();
                    slot.generation++;
                }

                slot.next_free_index = i + 1;
            }
        }

        if (capacity > 0)
            m_slots[capacity - 1].next_free_index = 0xFFFFFFFF;

        m_next_free_index = 0;
        m_size = 0;
    }

    void reserve(U32 capacity) {
        U32 old_capacity = static_cast<U32>(m_slots.size());
        FR_ASSERT(capacity > old_capacity, "New capacity must be greater than current capacity");
        if (capacity <= old_capacity)
            return;

        m_slots.reserve(capacity);

        for (U32 i = old_capacity; i < capacity; ++i) {
            m_slots.emplace_back();
            if (i == capacity - 1)
                m_slots[i].next_free_index = m_next_free_index;
            else
                m_slots[i].next_free_index = i + 1;
        }

        m_next_free_index = old_capacity;
    }

private:
    fr::DynamicArray<Slot<T>> m_slots;
    U32 m_next_free_index = 0xFFFFFFFF;
    U32 m_size{};
};

} // namespace fr
