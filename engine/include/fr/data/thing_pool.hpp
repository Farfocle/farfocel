/**
 * @file thing_pool.hpp
 * @author Kiju
 *
 * @brief ThingPool is a storage for things that provides safe ways to access and remove and handout
 * Things.
 */

#pragma once

#include <memory>

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/bitset.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {
class ThingPool {

public:
    using ThingArray = Array<Thing, MAX_THINGS>;
    using AliveBitset = Bitset<MAX_THINGS>;

    explicit ThingPool(Alloc *alloc = get_ambient_ctx().alloc) noexcept
        : m_alloc(alloc),
          m_buffer(static_cast<Byte *>(alloc->allocate(m_block_size, m_block_align))) {

        m_thing_array = std::construct_at(reinterpret_cast<ThingArray *>(m_buffer));
        m_alive_bitset =
            std::construct_at(reinterpret_cast<AliveBitset *>(m_buffer + m_alive_offset));

        m_alive_bitset->zero_all();
        m_alive_bitset->one_bit(0);
    }

    ~ThingPool() noexcept {
        std::destroy_at(m_alive_bitset);
        std::destroy_at(m_thing_array);
        m_alloc->deallocate(m_buffer, m_block_size, m_block_align);
    }

    ThingPool(const ThingPool &) = delete;
    ThingPool(ThingPool &&) = delete;
    ThingPool &operator=(const ThingPool &) = delete;
    ThingPool &operator=(ThingPool &&) = delete;

    /**
     * @brief Returns the allocator used by this pool.
     */
    const Alloc *alloc() const noexcept {
        return m_alloc;
    }

    /**
     * @brief Returns the capacity of this pool - the maximum number of things (MAX_THINGS).
     */
    USize capacity() const noexcept {
        return MAX_THINGS;
    }

    /**
     * @brief Returns the number of things currently alive.
     */
    USize alive_count() const noexcept {
        return m_load;
    }

    /**
     * @brief Returns the number of things currently dead.
     */
    USize dead_count() const noexcept {
        return MAX_THINGS - m_load;
    }

    /**
     * @brief Returns the number of things in the free list.
     */
    USize free_count() const noexcept {
        return m_next_free_count;
    }

    /**
     * @brief Hands out a new thing from the pool.
     * @note The returned thing is guaranteed to be alive.
     *
     * @warning Thread-unsafe.
     */
    Thing handout() noexcept {
        FR_ASSERT(m_load < MAX_THINGS, "pool is full");
        ThingArray &things = *m_thing_array;

        if (m_next_free_count == 0) {
            const ThingIdx idx = static_cast<ThingIdx>(m_load);
            const Thing out(idx, 0);

            things[idx] = out;
            m_alive_bitset->one_bit(idx);

            ++m_load;
            return out;
        }

        const ThingIdx idx = m_next_free_idx;
        Thing &slot = things[idx];

        --m_next_free_count;
        if (m_next_free_count == 0) {
            m_next_free_idx = 0;
        } else {
            m_next_free_idx = slot.idx();
        }

        slot.set_idx(idx);
        slot.inc_gen();
        m_alive_bitset->one_bit(idx);
        ++m_load;

        return slot;
    }

    /**
     * @brief Returns the thing stored at a slot index
     *
     * @param idx The index of the thing to get.
     * @pre idx < MAX_THINGS.
     *
     * @note If the thing is dead, returns a nil thing.
     */
    Thing get_by_idx(ThingIdx idx) const noexcept {
        FR_ASSERT(idx < MAX_THINGS, "idx out of bounds");

        if (!check_by_idx(idx)) {
            return Thing::nil();
        }

        return get_by_idx_unchecked(idx);
    }

    /**
     * @brief Returns the thing stored at a slot index
     *
     * @param idx The index of the thing to get.
     * @pre idx < MAX_THINGS.
     *
     * @warning Does not check whether the thing is alive or dead, may return garbage if the thing
     * is in the free list, may result in undefined behavior.
     */
    Thing get_by_idx_unchecked(ThingIdx idx) const noexcept {
        FR_ASSERT(idx < MAX_THINGS, "idx out of bounds");
        return (*m_thing_array)[idx];
    }

    /**
     * @brief Returns whether a thing is alive.
     * @note If the thing is nil, returns false.
     */
    bool check(Thing thing) const noexcept {
        if (!m_alive_bitset->check_bit(thing.idx())) [[unlikely]] {
            return false;
        }

        return get_by_idx_unchecked(thing.idx()).gen() == thing.gen();
    }

    /**
     * @brief Returns whether a thing is alive by its index. Does not check the generation.
     * @note If the thing is nil, returns false.
     * @param idx The index of the thing to check.
     * @pre idx < MAX_THINGS.
     * @return True if the thing is alive, false otherwise.
     */
    bool check_by_idx(ThingIdx idx) const noexcept {
        FR_ASSERT(idx < MAX_THINGS, "idx out of bounds");
        return m_alive_bitset->check_bit(idx);
    }

    /**
     * @brief Destroys a thing if it is valid.
     * @note If the thing is dead, does nothing.
     *
     * @warning Thread-unsafe.
     */
    void destroy(Thing thing) noexcept {
        if (!check(thing)) {
            return;
        }

        ThingArray &storage = *m_thing_array;
        const ThingIdx idx = thing.idx();

        m_alive_bitset->zero_bit(idx);
        storage[idx] = Thing(m_next_free_idx, thing.gen());

        m_next_free_idx = idx;
        ++m_next_free_count;
        --m_load;
    }

private:
    static constexpr USize m_storage_align = alignof(ThingArray);
    static constexpr USize m_alive_align = alignof(AliveBitset);
    static constexpr USize m_block_align =
        m_storage_align > m_alive_align ? m_storage_align : m_alive_align;
    static constexpr USize m_alive_offset =
        (sizeof(ThingArray) + m_alive_align - 1) & ~(m_alive_align - 1);
    static constexpr USize m_block_size = m_alive_offset + sizeof(AliveBitset);

    Alloc *m_alloc{get_ambient_ctx().alloc};

    Byte *m_buffer{nullptr};
    ThingArray *m_thing_array{nullptr};
    AliveBitset *m_alive_bitset{nullptr};

    // This is initialized to 1 to signal nil thing at index 0.
    USize m_load{1};

    ThingIdx m_next_free_idx{0};
    USize m_next_free_count{0};
};

} // namespace fr::impl
