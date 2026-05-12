/**
 * @file thing_pool.hpp
 * @author Kiju
 *
 * @brief ThingPool is a storage for things that provides safe ways to access and remove and handout
 * Things.
 */

#pragma once

#include <iostream>
#include <memory>

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/bitset.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr {
class ThingPool {
public:
    constexpr static USize total_capacity = 1 << 20;
    using Storage = Array<Thing, total_capacity>;
    using AliveBits = Bitset<total_capacity>;

    /**
     * @brief Constructs a ThingPool with the given allocator. If no allocator is provided, the
     * ambient context's allocator is used.
     */
    explicit ThingPool(Alloc *alloc = get_ambient_ctx().alloc) noexcept
        : m_alloc(alloc),
          m_block(static_cast<Byte *>(alloc->allocate(block_size, block_align))) {

        m_storage = std::construct_at(reinterpret_cast<Storage *>(m_block));
        m_alive = std::construct_at(reinterpret_cast<AliveBits *>(m_block + alive_offset));

        m_alive->zero_all();
    }

    /**
     * @brief Destroys the ThingPool, freeing its storage.
     */
    ~ThingPool() noexcept {
        std::destroy_at(m_alive);
        std::destroy_at(m_storage);
        m_alloc->deallocate(m_block, block_size, block_align);
    }

    /**
     * @brief Returns the allocator used by this pool.
     */
    const Alloc *alloc() const noexcept {
        return m_alloc;
    }

    /**
     * @brief Returns the capacity of this pool.
     */
    USize capacity() const noexcept {
        return total_capacity;
    }

    /**
     * @brief Returns the number of things currently handed out.
     */
    USize load() const noexcept {
        return m_load;
    }

    /**
     * @brief Returns a new thing from this pool.
     */
    Thing handout() noexcept {
        FR_ASSERT(m_load < total_capacity, "Pool is full");
        Storage &storage = *m_storage;

        if (m_next_free_count == 0) {
            const ThingIdx idx = static_cast<ThingIdx>(m_load);
            const Thing out(idx, 0);

            storage[idx] = out;
            m_alive->one_bit(idx);

            ++m_load;
            return out;
        }

        const ThingIdx idx = m_next_free_idx;
        Thing slot = storage[idx];

        --m_next_free_count;
        if (m_next_free_count == 0) {
            m_next_free_idx = 0;
        } else {
            m_next_free_idx = slot.idx();
        }

        ThingGen gen = static_cast<ThingGen>(slot.gen() + 1);

        if (gen == 0) {
            gen = 1;
        }

        const Thing out(idx, gen);
        storage[idx] = out;
        m_alive->one_bit(idx);

        return out;
    }

    /**
     * @brief Returns the thing stored at a slot index.
     */
    Thing get(ThingIdx idx) const noexcept {
        FR_ASSERT(idx < total_capacity, "index out of bounds");
        return (*m_storage)[idx];
    }

    /**
     * @brief Returns whether a thing is valid and alive.
     */
    bool check(Thing thing) const noexcept {
        if (thing.is_nil()) {
            return false;
        }

        if (thing.idx() >= total_capacity) {

            return false;
        }

        if (!m_alive->check_bit(thing.idx())) {

            return false;
        }

        const Thing slot = (*m_storage)[thing.idx()];
        return slot.gen() == thing.gen();
    }

    /**
     * @brief Removes a thing if it is valid.
     */
    bool remove(Thing thing) noexcept {
        if (!check(thing)) {
            return false;
        }

        Storage &storage = *m_storage;
        const ThingIdx idx = thing.idx();

        m_alive->zero_bit(idx);
        storage[idx] = Thing(m_next_free_idx, thing.gen());

        m_next_free_idx = idx;
        ++m_next_free_count;

        return true;
    }

private:
    static constexpr USize storage_align = alignof(Storage);
    static constexpr USize alive_align = alignof(AliveBits);
    static constexpr USize block_align = storage_align > alive_align ? storage_align : alive_align;
    static constexpr USize alive_offset = (sizeof(Storage) + alive_align - 1) & ~(alive_align - 1);
    static constexpr USize block_size = alive_offset + sizeof(AliveBits);

    Alloc *m_alloc{get_ambient_ctx().alloc};
    Byte *m_block{nullptr};
    Storage *m_storage{nullptr};
    AliveBits *m_alive{nullptr};

    /// @note This is initialized to 1 to signal nil thing.
    USize m_load{1};

    ThingIdx m_next_free_idx{0};
    USize m_next_free_count{0};
};
} // namespace fr
