/**
 * @file part_pool.hpp
 * @author Kiju
 *
 * @brief PartPool is responsible for storing and managing parts.
 */

#pragma once

#include <limits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/data/thing.hpp"

namespace fr {

/**
 * @brief PartPool is responsible for storing and managing parts.
 *
 * @tparam T The type of part to store.
 */
template <typename T>
class PartPool {
public:
    PartPool() noexcept
        : PartPool(get_ambient_ctx().alloc) {
    }

    explicit PartPool(Alloc *alloc) noexcept
        : m_alloc(alloc) {
        m_parts = DynamicArray<T>::with_alloc(alloc);
        m_sparse_indices = DynamicArray<ThingIdx>::with_alloc(alloc);
        m_dense_indices = DynamicArray<ThingIdx>::with_alloc(alloc);
    }

    ~PartPool() noexcept = default;

    /**
     * @brief Reserve space for a given number of parts.
     *
     * @param size The number of parts to reserve.
     * @warning Thread-unsafe.
     */
    void reserve_parts(USize size) noexcept {

        FR_ASSERT(size <= std::numeric_limits<ThingIdx>::max(), "size exceeds ThingIdx limit");
        m_parts.reserve(size);
        m_dense_indices.reserve(size);
    }

    /**
     * @brief Reserve space for a given number of lookup indices.
     *
     * @param size The number of indices to reserve.
     * @warning Thread-unsafe.
     */
    void reserve_lookup(USize size) noexcept {

        FR_ASSERT(size <= std::numeric_limits<ThingIdx>::max(), "size exceeds ThingIdx limit");
        m_sparse_indices.reserve(size);
    }

    /**
     * @brief Get the number of parts in the pool.
     *
     * @return The number of parts.
     */
    USize size() const noexcept {
        return m_parts.size();
    }

    /**
     * @brief Returns a view of all parts in dense order.
     */
    const DynamicArray<T> &parts() const noexcept {
        return m_parts;
    }

    /**
     * @brief Get a part by index.
     *
     * @param idx The index of the part to get.
     * @return A reference to the part.
     * @warning Unsafe: caller must ensure idx refers to a live part.
     * @warning Thread-unsafe.
     */
    T &get(ThingIdx idx) noexcept {
        FR_ASSERT(idx < m_sparse_indices.size(), "index out of bounds");
        return m_parts[m_sparse_indices[idx]];
    }

    /**
     * @brief Emplace a part at a given index.
     *
     * @param idx The index to emplace at.
     * @param args The arguments to forward to the emplace constructor.
     * @return A reference to the emplaced part.
     * @warning Unsafe: caller must ensure idx is unique and stable.
     * @warning Thread-unsafe.
     */
    template <typename... Args>
    T &emplace(ThingIdx idx, Args &&...args) noexcept {
        if (m_sparse_indices.size() <= idx) {
            m_sparse_indices.grow_default(idx + 1);
        }

        m_sparse_indices[idx] = m_parts.size();
        m_parts.emplace_back(std::forward<Args>(args)...);
        m_dense_indices.emplace_back(idx);
        return m_parts.back();
    }

    /**
     * @brief Insert a part at a given index.
     *
     * @param idx The index to insert at.
     * @param part The part to insert.
     * @return A reference to the inserted part.
     * @warning Unsafe: caller must ensure idx is unique and stable.
     * @warning Thread-unsafe.
     */
    T &insert(ThingIdx idx, T &&part) noexcept {
        return emplace(idx, std::forward<T>(part));
    }

    /**
     * @warning Unsafe: caller must ensure idx is unique and stable.
     * @warning Thread-unsafe.
     */
    T &insert(ThingIdx idx, const T &part) noexcept {
        return emplace(idx, part);
    }

    /**
     * @brief Remove a part at a given index.
     *
     * @param idx The index to remove at.
     * @warning Unsafe: caller must ensure idx refers to a live part.
     * @warning Thread-unsafe.
     */
    void remove(ThingIdx idx) noexcept {
        FR_ASSERT(idx < m_sparse_indices.size(), "index out of bounds");
        FR_ASSERT(m_parts.size() > 0, "remove on empty pool");

        ThingIdx rem_part_idx = m_sparse_indices[idx];
        ThingIdx rem_thing_idx = idx;

        ThingIdx swap_part_idx = m_parts.size() - 1;
        ThingIdx swap_thing_idx = m_dense_indices[swap_part_idx];

        if (rem_part_idx != swap_part_idx) {
            m_parts[rem_part_idx] = std::move(m_parts[swap_part_idx]);
            m_sparse_indices[swap_thing_idx] = rem_part_idx;
            m_dense_indices[rem_part_idx] = swap_thing_idx;
        }

        m_sparse_indices[rem_thing_idx] = 0;
        m_dense_indices.pop_back();
        m_parts.pop_back();
    }

private:
    Alloc *m_alloc{get_ambient_ctx().alloc};
    DynamicArray<T> m_parts{};

    /// @brief A sparse index array for looking the part index by the original thing index.
    DynamicArray<ThingIdx> m_sparse_indices{};

    /// @brief A dense index array for looking the original thing index of the part.
    DynamicArray<ThingIdx> m_dense_indices{};
};
} // namespace fr
