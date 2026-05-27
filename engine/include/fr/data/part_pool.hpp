/**
 * @file part_pool.hpp
 * @author Kiju
 *
 * @brief PartPool is a data structure responsible for storing and managing parts.
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/data/cmd.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {

/**
 * @brief PartPool is a data structure responsible for storing and managing parts.
 * @tparam T The type of part to store.
 * @pre T must be default constructible (stub).
 *
 * @note Implementation: PartPool consists of three dynamic arrays: m_parts, m_thing_to_part, and
 * m_part_to_thing.
 */
template <typename T>
    requires std::is_default_constructible_v<T>
class PartPool {
public:
    PartPool() noexcept
        : PartPool(get_ambient_ctx().alloc) {
    }

    explicit PartPool(Alloc *alloc) noexcept {
        m_alloc = alloc;
        m_parts = DynamicArray<T>::with_alloc(alloc);
        m_thing_to_part = DynamicArray<USize>::with_alloc(alloc);
        m_part_to_thing = DynamicArray<Thing>::with_alloc(alloc);

        m_destroy_cmds = DynamicArray<DestroyPartCmd<T>>::with_alloc(alloc);
        m_insert_cmds = DynamicArray<InsertPartCmd<T>>::with_alloc(alloc);
        m_mutate_cmds = DynamicArray<MutatePartCmd<T>>::with_alloc(alloc);

        m_parts.push_back(T());

        // Push the stub mapping to nil thing.
        m_thing_to_part.push_back(0);

        // Push the stub mapping from nil thing to the stub.
        m_part_to_thing.push_back(Thing::nil());
    }

    PartPool(const PartPool &) = delete;
    PartPool(PartPool &&) = delete;
    PartPool &operator=(const PartPool &) = delete;
    PartPool &operator=(PartPool &&) = delete;

    ~PartPool() noexcept = default;

    /**
     * @brief Reserves space for parts array and part -> thing lookup array.
     * @param size The number of parts to reserve.
     */
    void reserve_parts(USize size) noexcept {
        FR_ASSERT(size <= MAX_THINGS, "size exceeds ThingIdx limit");
        m_parts.reserve(size);
        m_part_to_thing.reserve(size);
    }

    /**
     * @brief Reserves space for part -> thing lookup array.
     * @param size The number of indices to reserve.
     */
    void reserve_lookup(USize size) noexcept {
        FR_ASSERT(size <= MAX_THINGS, "size exceeds ThingIdx limit");
        m_thing_to_part.reserve(size);
    }

    /**
     * @brief Get the number of parts in the pool including the stub.
     */
    USize part_count() const noexcept {
        return m_parts.size();
    }

    /**
     * @brief Returns a slice of parts including the stub.
     */
    Slice<const T> part_slice_with_stub() const noexcept {
        return m_parts.slice();
    }

    /**
     * @brief Returns a slice of parts excluding the stub.
     */
    Slice<const T> part_slice() const noexcept {
        return m_parts.slice_from(1);
    }

    /**
     * @brief Returns a slice mapping thing indices to part indices; includes the stub mapping.
     */
    Slice<const USize> thing_to_part_slice_with_stub() const noexcept {
        return m_thing_to_part.slice();
    }

    /**
     * @brief Returns a slice mapping thing indices to part indices; excludes the stub mapping.
     */
    Slice<const USize> thing_to_part_slice() const noexcept {
        return m_thing_to_part.slice_from(1);
    }

    /**
     * @brief Returns a slice mapping part indices to thing indices; includes the stub mapping.
     */
    Slice<const Thing> part_to_thing_slice_with_stub() const noexcept {
        return m_part_to_thing.slice();
    }

    /**
     * @brief Returns a slice mapping part indices to thing indices; excludes the stub mapping.
     */
    Slice<const Thing> part_to_thing_slice() const noexcept {
        return m_part_to_thing.slice_from(1);
    }

    /**
     * @brief Returns a reference to the stub.
     */
    T &get_stub() noexcept {
        return m_parts[0];
    }

    /**
     * @brief Returns a pointer to the part owned by the thing.
     * @note If thing is nil, returns a reference to the stub.
     * @warning Caller must ensure the thing is alive and DOES own part T.
     */
    T *get_unchecked(Thing thing) noexcept {
        FR_ASSERT(thing.idx() < m_thing_to_part.size(), "index out of bounds");
        return &m_parts[m_thing_to_part[thing.idx()]];
    }

    /**
     * @brief Emplace a part to a thing.
     * @return A reference to the emplaced part.
     * @warning Caller must ensure the thing is alive and DOES NOT own part T.
     */
    template <typename... Args>
    T &emplace_unchecked(Thing thing, Args &&...args) noexcept {
        ThingIdx idx = thing.idx();

        if (m_thing_to_part.size() <= idx) [[unlikely]] {
            m_thing_to_part.grow_default(idx + 1);
        }

        m_thing_to_part[idx] = m_parts.size();
        m_parts.emplace_back(std::forward<Args>(args)...);
        m_part_to_thing.emplace_back(thing);

        return m_parts.back();
    }

    /**
     * @brief Insert a part to a thing.
     * @return A reference to the inserted part.
     * @warning Caller must ensure the thing is alive and DOES NOT own part T.
     */
    T &insert_unchecked(Thing thing, T &&part) noexcept {
        return emplace_unchecked(thing, std::forward<T>(part));
    }

    /**
     * @brief Insert a part to a thing.
     * @return A reference to the inserted part.
     * @warning Caller must ensure the thing is alive and DOES NOT own part T.
     */
    T &insert_unchecked(Thing thing, const T &part) noexcept {
        return emplace_unchecked(thing, part);
    }

    /**
     * @brief Destroy a part owned by a thing.
     * @note If thing is nil, does nothing.
     * @warning Caller must ensure the thing is alive and DOES own part T.
     */
    void destroy_unchecked(Thing thing) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        USize idx = thing.idx();

        FR_ASSERT(idx < m_thing_to_part.size(), "index out of bounds");
        FR_ASSERT(m_parts.size() > 0, "remove on empty pool");

        USize rem_part_idx = m_thing_to_part[idx];
        USize rem_thing_idx = idx;

        USize swap_part_idx = m_parts.size() - 1;
        Thing swap_thing = m_part_to_thing[swap_part_idx];
        USize swap_thing_idx = swap_thing.idx();

        if (rem_part_idx != swap_part_idx) {
            m_parts[rem_part_idx] = std::move(m_parts[swap_part_idx]);
            m_thing_to_part[swap_thing_idx] = rem_part_idx;
            m_part_to_thing[rem_part_idx] = swap_thing;
        }

        m_thing_to_part[rem_thing_idx] = 0;
        m_part_to_thing.pop_back();
        m_parts.pop_back();
    }

    // ---------------------------------------------------------------- Commands

    /**
     * @brief Record a destroy command for a thing.
     */
    void record_destroy(Thing thing) noexcept {
        m_destroy_cmds.push_back(DestroyPartCmd<T>{.thing = thing});
    }

    /**
     * @brief Record an insert command for a thing.
     */
    void record_insert(Thing thing, const T &part) noexcept {
        m_insert_cmds.push_back(InsertPartCmd<T>{.thing = thing, .part = part});
    }

    /**
     * @brief Record a mutate command for a thing.
     */
    void record_mutate(Thing thing, const T &prev, const T &next) noexcept {
        m_mutate_cmds.push_back(MutatePartCmd<T>{.thing = thing, .prev = prev, .next = next});
    }

    /**
     * @brief Returns a read-only view of destroy commands.
     */
    Slice<const DestroyPartCmd<T>> destroy_cmds() const noexcept {
        return m_destroy_cmds.slice();
    }

    /**
     * @brief Returns a read-only view of insert commands.
     */
    Slice<const InsertPartCmd<T>> insert_cmds() const noexcept {
        return m_insert_cmds.slice();
    }

    /**
     * @brief Apply all recorded destroy commands.
     */
    void commit_destroy() noexcept {

        for (auto &cmd : m_destroy_cmds) {
            destroy_unchecked(cmd.thing);
        }

        m_destroy_cmds.clear();
    }

    /**
     * @brief Apply all recorded insert commands.
     */
    void commit_insert() noexcept {
        for (auto &cmd : m_insert_cmds) {
            insert_unchecked(cmd.thing, cmd.part);
        }

        m_insert_cmds.clear();
    }

    /**
     * @brief Apply all recorded mutate commands.
     */
    void commit_mutate() noexcept {
        for (auto &cmd : m_mutate_cmds) {
            T *part = get_unchecked(cmd.thing);
            *part = cmd.next;
        }

        m_mutate_cmds.clear();
    }

private:
    // -------------------------------------------------------- Member Variables
    Alloc *m_alloc{get_ambient_ctx().alloc};

    DynamicArray<T> m_parts{};

    // A sparse index array for looking the part index by the original thing index.
    DynamicArray<USize> m_thing_to_part{};

    // A dense index array for looking the original thing index of the part.
    DynamicArray<Thing> m_part_to_thing{};

    DynamicArray<DestroyPartCmd<T>> m_destroy_cmds{};
    DynamicArray<InsertPartCmd<T>> m_insert_cmds{};
    DynamicArray<MutatePartCmd<T>> m_mutate_cmds{};
};

FR_STATIC_ASSERT(sizeof(PartPool<Byte>) == sizeof(PartPool<U64>),
                 "part pools must have the same size regardless of the element type");

FR_STATIC_ASSERT(alignof(PartPool<Byte>) == alignof(PartPool<U64>),
                 "part pools must have the same alignment regardless of the element type");
} // namespace fr::impl
