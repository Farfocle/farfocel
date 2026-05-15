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
    using ThingsArray = Array<Thing, MAX_THINGS>;
    using AliveArray = Bitset<MAX_THINGS>;

    /**
     * @brief Constructs a ThingPool with the given allocator. If no allocator is provided, the
     * ambient context's allocator is used.
     */
    explicit ThingPool(Alloc *alloc = get_ambient_ctx().alloc) noexcept
        : m_alloc(alloc),
          m_buffer(static_cast<Byte *>(alloc->allocate(block_size, block_align))) {

        m_things = std::construct_at(reinterpret_cast<ThingsArray *>(m_buffer));
        m_alive = std::construct_at(reinterpret_cast<AliveArray *>(m_buffer + alive_offset));

        m_alive->zero_all();
    }

    /**
     * @brief Destroys the ThingPool, freeing its storage.
     */
    ~ThingPool() noexcept {
        std::destroy_at(m_alive);
        std::destroy_at(m_things);
        m_alloc->deallocate(m_buffer, block_size, block_align);
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
     * @brief Returns the capacity of this pool.
     */
    USize capacity() const noexcept {
        return MAX_THINGS;
    }

    /**
     * @brief The number of things currently alive.
     *
     * @return The number of things currently handed out.
     * @note Does include the default nil thing at index 0.
     */
    USize load() const noexcept {
        return m_load;
    }

    /**
     * @brief Returns a new thing from this pool.
     */
    Thing handout() noexcept {
        FR_ASSERT(m_load < MAX_THINGS, "pool is full");
        ThingsArray &things = *m_things;

        if (m_next_free_count == 0) {
            const ThingIdx idx = static_cast<ThingIdx>(m_load);
            const Thing out(idx, 0);

            things[idx] = out;
            m_alive->one_bit(idx);

            ++m_load;
            return out;
        }

        const ThingIdx idx = m_next_free_idx;
        Thing& slot = things[idx];

        --m_next_free_count;
        if (m_next_free_count == 0) {
            m_next_free_idx = 0;
        } else {
            m_next_free_idx = slot.idx();
        }

        slot.set_idx(idx);
        slot.inc_gen();
        m_alive->one_bit(idx);
        ++m_load;

        return slot;
    }

    /**
     * @brief Returns the thing stored at a slot index.
     */
    Thing get(ThingIdx idx) const noexcept {
        FR_ASSERT(idx < MAX_THINGS, "index out of bounds");
        return (*m_things)[idx];
    }

    /**
     * @brief Returns whether a thing is alive.
     */
    bool check(Thing thing) const noexcept {
        if (!m_alive->check_bit(thing.idx())) {
            return false;
        }

        return get(thing.idx()).gen() == thing.gen();
    }

    /**
     * @brief Destroys a thing if it is valid.
     * @return If the thing was destroyed returns true, if not (the thing does not exist or is not
     * valid) returns false.
     */
    bool destroy(Thing thing) noexcept {
        if (!check(thing)) {
            return false;
        }

        ThingsArray &storage = *m_things;
        const ThingIdx idx = thing.idx();

        m_alive->zero_bit(idx);
        storage[idx] = Thing(m_next_free_idx, thing.gen());

        m_next_free_idx = idx;
        ++m_next_free_count;
        --m_load;

        return true;
    }

private:
    static constexpr USize storage_align = alignof(ThingsArray);
    static constexpr USize alive_align = alignof(AliveArray);
    static constexpr USize block_align = storage_align > alive_align ? storage_align : alive_align;
    static constexpr USize alive_offset =
        (sizeof(ThingsArray) + alive_align - 1) & ~(alive_align - 1);
    static constexpr USize block_size = alive_offset + sizeof(AliveArray);

    Alloc *m_alloc{get_ambient_ctx().alloc};
    Byte *m_buffer{nullptr};
    ThingsArray *m_things{nullptr};
    AliveArray *m_alive{nullptr};

    /// @note This is initialized to 1 to signal nil thing at index 0.
    USize m_load{1};

    ThingIdx m_next_free_idx{0};
    USize m_next_free_count{0};
};

} // namespace fr::impl
