/**
 * @file thing_pool.hpp
 * @author Kiju
 *
 * @brief ThingPool is a storage for things that provides safe ways to access and remove and handout
 * Things.
 */

#pragma once

#include <cstring>

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {
class ThingPool {

public:
    ThingPool() noexcept
        : ThingPool(get_ambient_ctx().alloc) {
    }

    explicit ThingPool(Alloc *alloc) noexcept {
        using Things = Array<Thing, MAX_THINGS>;

        m_alloc = alloc;

        void *raw = m_alloc->allocate(sizeof(Things), alignof(Things));
        m_things = static_cast<Things *>(raw);

        // Uses memset to zero-initialize the array - the fastest way to clear memory
        std::memset(m_things, 0, sizeof(Things));
    }

    ~ThingPool() noexcept {
        using Things = Array<Thing, MAX_THINGS>;

        m_alloc->deallocate(m_things, sizeof(Things), alignof(Things));
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
        return m_alive_count;
    }

    /**
     * @brief Returns the number of things currently dead.
     */
    USize dead_count() const noexcept {
        return MAX_THINGS - m_alive_count;
    }

    /**
     * @brief Returns the number of things in the free list.
     */
    USize free_count() const noexcept {
        return m_free_count;
    }

    /**
     * @brief Return a new, fresh and non-nil thing.
     */
    Thing handout() noexcept {
        if (m_free_count == 0) {
            return do_handout_from_back();
        } else {
            return do_handout_from_free();
        }
    }

    /**
     * @brief Kills an alive thing.
     * @note If thing is nil, does nothing, nil thing is immortal.
     * @warning Caller is responsible for checking if a thing is alive or not.
     */
    void kill_alive(Thing thing) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        do_kill_unchecked(thing);
    }

    /**
     * @brief Kills a thing.
     * @note If thing is nil, does nothing, nil thing is immortal.
     * @note If thing is not alive, does nothing.
     */
    void kill(Thing thing) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!check(thing)) [[unlikely]] {
            return;
        }

        do_kill_unchecked(thing);
    }

    /**
     * @brief Checks if a thing is alive or dead.
     * @note The nil thing is alive and immortal.
     */
    bool check(Thing thing) const noexcept {
        auto &things = *m_things;
        return thing.gen() == things[thing.idx()].gen();
    }

private:
    Thing do_handout_from_back() noexcept {
        auto &things = *m_things;
        things[m_alive_count] = Thing(m_alive_count, 0);

        ++m_alive_count;
        return things[m_alive_count - 1];
    }

    Thing do_handout_from_free() noexcept {
        auto &things = *m_things;

        ThingIdx fresh_idx = m_free_next;
        ThingGen fresh_gen = things[fresh_idx].gen();
        ThingIdx next_idx = things[fresh_idx].idx();

        m_free_next = next_idx;
        --m_free_count;

        ++m_alive_count;
        return Thing(fresh_idx, fresh_gen);
    }

    void do_kill_unchecked(Thing thing) noexcept {
        auto &things = *m_things;
        auto &it = things[thing.idx()];

        it.set_idx(m_free_next);
        it.inc_gen();

        m_free_next = thing.idx();
        ++m_free_count;
    }

    Alloc *m_alloc{nullptr};
    Array<Thing, MAX_THINGS> *m_things{nullptr};
    USize m_alive_count{1};
    USize m_free_count{0};
    USize m_free_next{0};
};
} // namespace fr::impl
