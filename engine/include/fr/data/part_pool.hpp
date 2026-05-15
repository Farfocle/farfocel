/**
 * @file part_pool.hpp
 * @author Kiju
 *
 * @brief PartPool is responsible for storing and managing parts.
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/nil.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {

/**
 * @brief PartPool is responsible for storing and managing parts.
 *
 * @tparam T The type of part to store.
 * @pre T must be default-constructible or nillable.
 *
 * @warning Thread-unsafe.
 */
template <typename T>
    requires std::is_default_constructible_v<T> || IsNillable<T>
class PartPool {
public:
    PartPool() noexcept
        : PartPool(get_ambient_ctx().alloc) {
    }

    explicit PartPool(Alloc *alloc) noexcept
        : m_alloc(alloc) {
        m_parts = DynamicArray<T>::with_alloc(alloc);
        m_thing_to_part = DynamicArray<ThingIdx>::with_alloc(alloc);
        m_part_to_thing = DynamicArray<ThingIdx>::with_alloc(alloc);

        if constexpr (std::is_default_constructible_v<T>) {
            m_parts.push_back(T());
        } else {
            m_parts.push_back(call_nil<T>());
        }

        m_thing_to_part.push_back(0);
        m_part_to_thing.push_back(0);
    }

    PartPool(const PartPool &) = delete;
    PartPool(PartPool &&) = delete;
    PartPool &operator=(const PartPool &) = delete;
    PartPool &operator=(PartPool &&) = delete;

    ~PartPool() noexcept = default;

    /**
     * @brief Reserve space for a given number of parts.
     *
     * @param size The number of parts to reserve.
     * @warning Thread-unsafe.
     */
    void reserve_parts(USize size) noexcept {
        FR_ASSERT(size <= MAX_THINGS, "size exceeds ThingIdx limit");

        m_parts.reserve(size);
        m_part_to_thing.reserve(size);
    }

    /**
     * @brief Reserve space for a given number of lookup indices.
     *
     * @param size The number of indices to reserve.
     * @warning Thread-unsafe.
     */
    void reserve_lookup(USize size) noexcept {
        FR_ASSERT(size <= MAX_THINGS, "size exceeds ThingIdx limit");
        m_thing_to_part.reserve(size);
    }

    /**
     * @brief Get the number of parts in the pool.
     *
     * @return The number of parts.
     * @note Does include the default part at index 0.
     */
    USize load() const noexcept {
        return m_parts.size();
    }

    /**
     * @brief Returns a dynamic array of all parts in dense order.
     */
    const DynamicArray<T> &parts_array() const noexcept {
        return m_parts;
    }

    /**
     * @brief Returns a dynamic array mapping part indices to thing indices.
     */
    const DynamicArray<ThingIdx> &part_to_thing_array() const noexcept {
        return m_part_to_thing;
    }

    /**
     * @brief Returns a dynamic array mapping thing indices to part indices.
     */
    const DynamicArray<ThingIdx> &thing_to_part_array() const noexcept {
        return m_thing_to_part;
    }

    /**
     * @brief Get a part by index.
     *
     * @param idx The index of the part to get.
     * @return A reference to the part.
     *
     * @note If idx is zero, then returns a reference to the first part - stub.
     * @warning Caller must ensure idx refers to a live part.
     * @warning Thread-unsafe.
     */
    T &get(ThingIdx idx) noexcept {
        FR_ASSERT(idx < m_thing_to_part.size(), "index out of bounds");

        return m_parts[m_thing_to_part[idx]];
    }

    /**
     * @brief Emplace a part at a given index.
     *
     * @param idx The index to emplace at.
     * @param args The arguments to forward to the emplace constructor.
     * @return A reference to the emplaced part.
     *
     * @pre idx must be non-zero.
     * @warning Caller must ensure idx is unique and non-zero.
     * @warning Thread-unsafe.
     */
    template <typename... Args>
    T &emplace(ThingIdx idx, Args &&...args) noexcept {
        FR_ASSERT(idx != 0, "idx must be non-zero");

        if (m_thing_to_part.size() <= idx) {
            m_thing_to_part.grow_default(idx + 1);
        }

        m_thing_to_part[idx] = m_parts.size();
        m_parts.emplace_back(std::forward<Args>(args)...);
        m_part_to_thing.emplace_back(idx);

        return m_parts.back();
    }

    /**
     * @brief Insert a part at a given index.
     *
     * @param idx The index to insert at.
     * @param part The part to insert.
     * @return A reference to the inserted part.
     *
     * @pre idx must be non-zero.
     * @warning Caller must ensure idx is unique and non-zero.
     * @warning Thread-unsafe.
     */
    T &insert(ThingIdx idx, T &&part) noexcept {
        return emplace(idx, std::forward<T>(part));
    }

    /**
     * @brief Insert a part at a given index.
     *
     * @param idx The index to insert at.
     * @param part The part to insert.
     * @return A reference to the inserted part.
     *
     * @pre idx must be non-zero.
     * @warning Caller must ensure idx is unique and non-zero.
     * @warning Thread-unsafe.
     */
    T &insert(ThingIdx idx, const T &part) noexcept {
        return emplace(idx, part);
    }

    /**
     * @brief Destroy a part at a given index.
     *
     * @param idx The index to remove at.
     *
     * @note If idx must be non-zero.
     * @warning Caller must ensure idx refers to a live part as is non-zero.
     * @warning Thread-unsafe.
     */
    void destroy(ThingIdx idx) noexcept {
        FR_ASSERT(idx != 0, "idx must be non-zero");
        FR_ASSERT(idx < m_thing_to_part.size(), "index out of bounds");
        FR_ASSERT(m_parts.size() > 0, "remove on empty pool");

        ThingIdx rem_part_idx = m_thing_to_part[idx];
        ThingIdx rem_thing_idx = idx;

        ThingIdx swap_part_idx = m_parts.size() - 1;
        ThingIdx swap_thing_idx = m_part_to_thing[swap_part_idx];

        if (rem_part_idx != swap_part_idx) {
            m_parts[rem_part_idx] = std::move(m_parts[swap_part_idx]);
            m_thing_to_part[swap_thing_idx] = rem_part_idx;
            m_part_to_thing[rem_part_idx] = swap_thing_idx;
        }

        m_thing_to_part[rem_thing_idx] = 0;
        m_part_to_thing.pop_back();
        m_parts.pop_back();
    }

private:
    Alloc *m_alloc{get_ambient_ctx().alloc};
    DynamicArray<T> m_parts{};

    /// @brief A sparse index array for looking the part index by the original thing index.
    DynamicArray<ThingIdx> m_thing_to_part{};

    /// @brief A dense index array for looking the original thing index of the part.
    DynamicArray<ThingIdx> m_part_to_thing{};
};

FR_STATIC_ASSERT(sizeof(PartPool<Byte>) == sizeof(PartPool<U64>),
                 "part pools must have the same size regardless of the element type");

FR_STATIC_ASSERT(alignof(PartPool<Byte>) == alignof(PartPool<U64>),
                 "part pools must have the same alignment regardless of the element type");
} // namespace fr::impl
