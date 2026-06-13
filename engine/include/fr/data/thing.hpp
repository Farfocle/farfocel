/**
 * @file thing.hpp
 * @author Kiju
 *
 * @brief Thing represents a universal handle to all game objects. By default, it is 32 bits wide.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/hash.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

// ======================================================== Typedefs & Constants

using ThingRaw = U32;
using ThingIdx = U32;
using ThingGen = U32;

constexpr USize THING_IDX_BITS = 20;
constexpr USize THING_GEN_BITS = 12;
constexpr USize THING_RAW_BITS = THING_IDX_BITS + THING_GEN_BITS;
constexpr ThingIdx THING_MAX_IDX = 0xFFFFF;
constexpr ThingGen THING_MAX_GEN = 0xFFF;
constexpr USize MAX_THINGS = 1 << THING_IDX_BITS;

FR_STATIC_ASSERT(sizeof(ThingRaw) * 2 <= sizeof(ThingIdx) + sizeof(ThingGen),
                 "`ThingRaw` is too large for the index and generation fields");

// ======================================================================= Thing

/**
 * @brief Thing represents a universal handle to all game objects. By default, it is 32 bits wide.
 * @note The index field is 20 bits wide, and the generation field is 12 bits wide. The nil value
 * for the thing is 0. Nil thing is valid but it acts as a no-op.
 */
struct Thing {
public:
    // ------------------------------------------------------------ Constructors

    constexpr Thing() noexcept = default;
    explicit constexpr Thing(ThingIdx idx, ThingGen gen) noexcept {
        m_thing = (gen << THING_IDX_BITS) | idx;
    }

    static constexpr Thing from_raw(ThingRaw raw) noexcept {
        return Thing(raw);
    }

    // --------------------------------------------------------------------- API

    /// @brief Returns the index part of the thing.
    constexpr ThingIdx idx() const noexcept {
        return m_thing & 0xFFFFF;
    }

    /// @brief Returns the generation part of the thing.
    constexpr ThingGen gen() const noexcept {
        return m_thing >> THING_IDX_BITS;
    }

    /// @brief Sets the index part of the thing.
    constexpr void set_idx(ThingIdx idx) noexcept {
        m_thing = (m_thing & 0xFFF00000) | idx;
    }

    /// @brief Sets the generation part of the thing.
    constexpr void set_gen(ThingGen gen) noexcept {
        m_thing = (m_thing & 0x000FFFFF) | (gen << THING_IDX_BITS);
    }

    /// @brief Increments generation, wraps around, skipping 0.
    constexpr void inc_gen() noexcept {
        set_gen((gen() + 1) % (THING_MAX_GEN + 1));
    }

    /// @brief Returns the raw value of the thing as U32.
    constexpr ThingRaw as_raw() const noexcept {
        return m_thing;
    }

    /// @brief Returns whether the thing is nil.
    constexpr bool operator==(const Thing &other) const noexcept {
        return m_thing == other.m_thing;
    }

    /// @brief Returns whether the thing is not nil.
    constexpr bool operator!=(const Thing &other) const noexcept {
        return !(*this == other);
    }

    /// @brief Returns the nil value of the thing.
    static constexpr Thing nil() noexcept {
        return Thing(0);
    }

    /// @brief Returns whether the thing is nil.
    constexpr bool is_nil() const noexcept {
        return m_thing == 0;
    }

    /// @brief Hash function for Thing (splitmix64 on the raw 32-bit value).
    Hash hash() const noexcept {
        return Hash::from_raw(impl::splitmix64(static_cast<U64>(m_thing)));
    }

    /// @brief Serializes the thing to an archive.
    template <typename Archive>
    void shape(Archive &archive) noexcept {
        if constexpr (Archive::action == ArchiveAction::Write) {
            ThingIdx idx_value = idx();
            ThingGen gen_value = gen();

            archive.prop("idx", idx_value);
            archive.prop("gen", gen_value);
        } else {
            ThingIdx idx{0};
            ThingGen gen{0};

            archive.prop("idx", idx);
            archive.prop("gen", gen);

            FR_ASSERT(idx <= THING_MAX_IDX, "invalid thing index");
            FR_ASSERT(gen <= THING_MAX_GEN, "invalid thing generation");

            set_idx(idx);
            set_gen(gen);
        }
    }

private:
    // --------------------------------------------------------------- Internals

    explicit constexpr Thing(ThingRaw raw) noexcept
        : m_thing(raw) {
    }

    // ----------------------------------------------------------------- Members
    U32 m_thing{0};
};

// =================================================================== ThingPool

namespace impl {
class ThingPool {
public:
    // ----------------------------------- Constructors & Operators & Destructor

    ThingPool() noexcept
        : ThingPool(get_ambient_ctx().alloc) {
    }

    explicit ThingPool(Alloc *alloc) noexcept {
        using Things = Array<Thing, MAX_THINGS>;

        m_alloc = alloc;

        void *raw = m_alloc->allocate(sizeof(Things), alignof(Things));
        m_things = static_cast<Things *>(raw);

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

    // --------------------------------------------------------------------- API

    /// @brief Returns the allocator used by this pool.
    const Alloc *alloc() const noexcept {
        return m_alloc;
    }

    /// @brief Returns the capacity of this pool - the maximum number of things (MAX_THINGS).
    USize capacity() const noexcept {
        return MAX_THINGS;
    }

    /// @brief Returns the number of things currently alive.
    USize alive_count() const noexcept {
        return m_alive_count;
    }

    /// @brief Returns the number of things currently dead.
    USize dead_count() const noexcept {
        return MAX_THINGS - m_alive_count;
    }

    /// @brief Returns the number of things in the free list.
    USize free_count() const noexcept {
        return m_free_count;
    }

    /// @brief Return a new, fresh and non-nil thing.
    Thing spawn() noexcept {
        if (m_free_count == 0) {
            return do_spawn_from_back();
        } else {
            return do_spawn_from_free();
        }
    }

    /// @brief Return a thing at given index.
    Thing get_by_idx(ThingIdx idx) const noexcept {
        return (*m_things)[idx];
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

    // --------------------------------------------------------------- Protocols

    /**
     * @brief Compact shape protocol for ThingPool.
     * @note Serializes alive_count, free_count, free_next, and the used portion of the slots array.
     * @note Signatures are derived state (set by PartPool inserts) — not serialized here.
     */
    template <typename Archive>
    void shape(Archive &archive) noexcept {
        if constexpr (Archive::action == ArchiveAction::Write) {
            archive.prop("alive_count", m_alive_count);
            archive.prop("free_count", m_free_count);
            archive.prop("free_next", m_free_next);
            archive.list("slots", [&](Archive &la) {
                for (USize i = 0; i < m_alive_count; ++i) {
                    ThingRaw raw = (*m_things)[i].as_raw();
                    la.prop("", raw);
                }
            });
        } else {
            archive.prop("alive_count", m_alive_count);
            archive.prop("free_count", m_free_count);
            archive.prop("free_next", m_free_next);
            using Things = Array<Thing, MAX_THINGS>;
            std::memset(m_things, 0, sizeof(Things));
            archive.list("slots", [&](Archive &la) {
                USize n = la.current_list_size();
                for (USize i = 0; i < n; ++i) {
                    ThingRaw raw = 0;
                    la.prop("", raw);
                    (*m_things)[i] = Thing::from_raw(raw);
                }
            });
        }
    }

private:
    // --------------------------------------------------------------- Internals

    Thing do_spawn_from_back() noexcept {
        auto &things = *m_things;

        things[m_alive_count] = Thing(m_alive_count, 0);

        ++m_alive_count;
        return things[m_alive_count - 1];
    }

    Thing do_spawn_from_free() noexcept {
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
        --m_alive_count;
    }

    // ----------------------------------------------------------------- Members
    Alloc *m_alloc{nullptr};
    Array<Thing, MAX_THINGS> *m_things{nullptr};
    USize m_alive_count{1};
    USize m_free_count{0};
    USize m_free_next{0};
};
} // namespace impl
} // namespace fr
