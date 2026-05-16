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
#include "fr/core/slice.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {

/**
 * @brief PartPool is responsible for storing and managing parts.
 *
 * @tparam T The type of part to store.
 *
 * @pre T must be default-constructible or nillable.
 * @warning Mutations are thread-unsafe.
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

        // Push the stub mapping to nil thing.
        m_thing_to_part.push_back(0);

        // Push the stub mapping from nil thing to the stub.
        m_part_to_thing.push_back(0);
    }

    PartPool(const PartPool &) = delete;
    PartPool(PartPool &&) = delete;
    PartPool &operator=(const PartPool &) = delete;
    PartPool &operator=(PartPool &&) = delete;

    ~PartPool() noexcept = default;

    /**
     * @brief Reserve space for a given number of parts.
     * @note Reserves space for parts array and part -> thing lookup array.
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
     * @note Reserves space for part -> thing lookup array.
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
     * @note Includes the stub at index 0.
     */
    USize part_count() const noexcept {
        return m_parts.size();
    }

    /**
     * @brief Returns a slice of all parts.
     * @note Includes the stub at index 0.
     */
    Slice<const T> part_slice_with_stub() const noexcept {
        return m_parts.slice();
    }

    /**
     * @brief Returns a slice of all parts.
     * @note Excludes the stub at index 0.
     */
    Slice<const T> part_slice() const noexcept {
        return m_parts.slice_from(1);
    }

    /**
     * @brief Returns a slice mapping thing indices to part indices.
     * @note Includes the mapping from the nil thing to the stub.
     */
    Slice<const ThingIdx> thing_to_part_slice_with_stub() const noexcept {
        return m_thing_to_part.slice();
    }

    /**
     * @brief Returns a slice mapping thing indices to part indices.
     * @note Excludes the mapping from the nil thing to the stub.
     */
    Slice<const ThingIdx> thing_to_part_slice() const noexcept {
        return m_thing_to_part.slice_from(1);
    }

    /**
     * @brief Returns a slice mapping part indices to thing indices.
     * @note Includes the mapping from the stub to the nil thing.
     */
    Slice<const ThingIdx> part_to_thing_slice_with_stub() const noexcept {
        return m_part_to_thing.slice();
    }

    /**
     * @brief Returns a slice mapping part indices to thing indices.
     * @note Excludes the mapping from the stub to the nil thing.
     */
    Slice<const ThingIdx> part_to_thing_slice() const noexcept {
        return m_part_to_thing.slice_from(1);
    }

    /**
     * @brief Returns a reference to the stub.
     */
    const T &stub() const noexcept {
        return m_parts[0];
    }

    /**
     * @brief Returns a reference to the stub.
     * @warning Thread-unsafe. Caller must ensure no concurrent access.
     */
    T &stub_mut() noexcept {
        return m_parts[0];
    }

    /**
     * @brief Returns a reference to the part associated with a given thing.
     * @note If thing is nil, returns a reference to the stub.
     *
     * @warning Caller must ensure the thing is alive and has the pool's part attached.
     */
    const T &get_unchecked(Thing thing) noexcept {
        FR_ASSERT(thing.idx() < m_thing_to_part.size(), "index out of bounds");
        return m_parts[m_thing_to_part[thing.idx()]];
    }

    /**
     * @brief Returns a reference to the part associated with a given thing.
     * @note If thing is nil, returns a reference to the stub.
     *
     * @warning Caller must ensure the thing is alive and has the pool's part attached.
     * @warning Thread-unsafe. Caller must ensure no concurrent access.
     */
    T &get_mut_unchecked(Thing thing) noexcept {
        FR_ASSERT(thing.idx() < m_thing_to_part.size(), "index out of bounds");
        return m_parts[m_thing_to_part[thing.idx()]];
    }

    /**
     * @brief Emplace a part at a given index.
     *
     * @tparam Args The types of the arguments to forward.
     * @param thing The thing.
     * @param args The arguments to forward to the emplace constructor.
     *
     * @return A reference to the emplaced part.
     *
     * @warning Caller must ensure idx is unique, otherwise behavior is undefined.
     * @warning Caller must ensure there is no existing part of this type attached to the thing,
     * otherwise behavior is undefined.
     * @warning Thread-unsafe.
     */
    template <typename... Args>
    T &emplace_unchecked(Thing thing, Args &&...args) noexcept {
        ThingIdx idx = thing.idx();

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
     * @param thing The thing.
     * @param part The part to insert.
     * @return A reference to the inserted part.
     *
     * @warning Caller must ensure idx is unique, otherwise behavior is undefined.
     * @warning Caller must ensure there is no existing part of this type attached to the thing,
     * otherwise behavior is undefined.
     * @warning Thread-unsafe.
     */
    T &insert_unchecked(Thing thing, T &&part) noexcept {
        return emplace_unchecked(thing, std::forward<T>(part));
    }

    /**
     * @brief Insert a part at a given index.
     *
     * @param thing The thing.
     * @param part The part to insert.
     * @return A reference to the inserted part.
     *
     * @warning Caller must ensure idx is unique, otherwise behavior is undefined.
     * @warning Caller must ensure there is no existing part of this type attached to the thing,
     * otherwise behavior is undefined.
     * @warning Thread-unsafe.
     */
    T &insert_unchecked(Thing thing, const T &part) noexcept {
        return emplace_unchecked(thing, part);
    }

    /**
     * @brief Destroy a part attached to a given thing.
     *
     * @param thing The thing to destroy the part of.
     * @pre Thing is non-nil
     *
     * @warning Caller must ensure idx refers to a live part, otherwise behavior is undefined.
     * @warning Caller must ensure there is an existing part of this type attached to the thing,
     * otherwise behavior is undefined.
     * @warning Thread-unsafe.
     */
    void destroy(Thing thing) noexcept {
        ThingIdx idx = thing.idx();

        FR_ASSERT(idx != 0, "destroying nil thing is not allowed");
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

    // A sparse index array for looking the part index by the original thing index.
    DynamicArray<ThingIdx> m_thing_to_part{};

    // A dense index array for looking the original thing index of the part.
    DynamicArray<ThingIdx> m_part_to_thing{};
};

FR_STATIC_ASSERT(sizeof(PartPool<Byte>) == sizeof(PartPool<U64>),
                 "part pools must have the same size regardless of the element type");

FR_STATIC_ASSERT(alignof(PartPool<Byte>) == alignof(PartPool<U64>),
                 "part pools must have the same alignment regardless of the element type");
} // namespace fr::impl
