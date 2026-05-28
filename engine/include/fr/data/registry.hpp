/**
 * @file registry.hpp
 * @author Kiju
 *
 * @brief Registry is a center of operations regarding things and parts in Farfocel.
 */

#pragma once

#include <utility>

#include "fr/core/inline_any.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/part.hpp"
#include "fr/data/part_pool.hpp"
#include "fr/data/signature_pool.hpp"
#include "fr/data/thing_pool.hpp"

namespace fr {
template <typename... Include>
class Query;
}

namespace fr::impl {
class Registry {
    template <typename... Include>
    friend class fr::Query;

public:
    using AnyPartPool = InlineAny<sizeof(PartPool<Byte>), alignof(PartPool<Byte>)>;

    // --------------------------------------------- Constructors and Destructor
    Registry() noexcept
        : m_alloc(get_ambient_ctx().alloc) {};

    explicit Registry(Alloc *alloc) noexcept
        : m_alloc(alloc),
          m_thing_pool(alloc),
          m_signature_pool(alloc) {};

    Registry(const Registry &) = delete;
    Registry(Registry &&) = delete;
    Registry &operator=(const Registry &) = delete;
    Registry &operator=(Registry &&) = delete;

    // ------------------------------------------------- Internal Storage Access

    /**
     * @brief Returns the allocator used by this registry.
     */
    const Alloc *alloc() const noexcept {
        return m_alloc;
    }

    /**
     * @brief Returns the thing pool used by this registry.
     */
    const ThingPool &thing_pool() const noexcept {
        return m_thing_pool;
    }

    /**
     * @brief Returns the signature pool used by this registry.
     */
    const SignaturePool &signature_pool() const noexcept {
        return m_signature_pool;
    }

    /**
     * @brief Returns the part pool of type T used by this registry.
     * @note Returns nullptr if the pool does not exist.
     */
    template <typename T>
    const PartPool<T> *part_pool() const noexcept {
        return do_part_pool<T>();
    }

    // -------------------------------------------------------- Thing Operations

    /**
     * @brief Returns a fresh, non-nil thing.
     */
    Thing handout() noexcept {
        return m_thing_pool.handout();
    }

    /**
     * @brief Kills a thing, adding its slot into a free list.
     * @note If a thing is nil, does nothing, nil thing is immortal.
     * @note If a thing is dead, the pool does nothing but the signature is reset.
     */
    void kill(Thing thing) noexcept {
        m_thing_pool.kill(thing);
        m_signature_pool.destroy_all(thing);
    }

    /**
     * @brief Checks if a thing is alive.
     * @note The nil thing is alive and immortal.
     */
    bool is_alive(Thing thing) const noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return true;
        }

        return m_thing_pool.check(thing);
    }

    /**
     * @brief Checks if a thing is dead.
     * @note The nil thing is alive and immortal.
     */
    bool is_dead(Thing thing) const noexcept {
        return !is_alive(thing);
    }

    // --------------------------------------------------------- Part Operations

    /**
     * @brief Checks if a part pool of type `T` exists.
     * @return True if the pool exists, false otherwise.
     */
    template <typename T>
    bool check_part_pool() const noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        return !do_check_part_pool(tidx);
    }

    /**
     * @brief Checks if a thing owns part `T`.
     * @note Returns false if the pool is missing or the thing is dead.
     * @note Returns true for nil thing if the pool exists.
     */
    template <typename T>
    bool has(Thing thing) const noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        if (do_check_part_pool(tidx)) [[unlikely]] {
            return false;
        }

        if (do_check_thing_nil(thing)) [[unlikely]] {
            return true;
        }

        if (!do_check_thing_alive(thing)) [[unlikely]] {
            return false;
        }

        return do_check_part(thing, tidx);
    }

    /**
     * @brief Tries to emplace part `T` on a thing.
     * @note Creates the part pool if missing.
     * @note Returns the stub for nil thing.
     * @note Returns nullptr if the thing is dead or already owns `T`.
     */
    template <typename T, typename... Args>
    T *try_emplace(Thing thing, Args &&...args) noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        PartPool<T> &pool = do_ensure_part_pool<T>(tidx);

        if (do_check_thing_nil(thing)) [[unlikely]] {
            return &pool.get_stub();
        }

        if (!do_check_thing_alive(thing)) [[unlikely]] {
            return nullptr;
        }

        if (do_check_part(thing, tidx)) [[unlikely]] {
            return nullptr;
        }

        m_signature_pool.insert(thing, tidx);
        return &pool.emplace_unchecked(thing, std::forward<Args>(args)...);
    }

    /**
     * @brief Tries to insert part `T` on a thing by const reference.
     * @note Same behavior as `try_emplace`.
     */
    template <typename T>
    T *try_insert(Thing thing, const T &part) noexcept {
        return try_emplace<T>(thing, part);
    }

    /**
     * @brief Tries to insert part T on a thing by rvalue.
     * @note Same behavior as try_emplace_part.
     */
    template <typename T>
    T *try_insert(Thing thing, T &&part) noexcept {
        return try_emplace<T>(thing, std::move(part));
    }

    /**
     * @brief Emplaces part `T` on a thing.
     * @note Creates the part pool if missing.
     * @note Returns the stub for nil thing.
     * @warning Asserts if the thing is dead or already owns `T`.
     */
    template <typename T, typename... Args>
    T &emplace(Thing thing, Args &&...args) noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        PartPool<T> &pool = do_ensure_part_pool<T>(tidx);

        if (do_check_thing_nil(thing)) [[unlikely]] {
            return pool.get_stub();
        }

        FR_ASSERT(do_check_thing_alive(thing), "cannot emplace part T on a dead thing");
        FR_ASSERT(!do_check_part(thing, tidx),
                  "cannot emplace part T if a thing already owns a part T");

        m_signature_pool.insert(thing, tidx);
        return pool.emplace_unchecked(thing, std::forward<Args>(args)...);
    }

    /**
     * @brief Inserts part `T` on a thing by const reference.
     * @note Same behavior as `emplace_part`.
     */
    template <typename T>
    T &insert(Thing thing, const T &part) noexcept {
        return emplace<T>(thing, part);
    }

    /**
     * @brief Inserts part `T` on a thing by rvalue.
     * @note Same behavior as emplace_part.
     */
    template <typename T>
    T &insert(Thing thing, T &&part) noexcept {
        return emplace<T>(thing, std::move(part));
    }

    /**
     * @brief Tries to destroy part `T` on a thing.
     * @note Returns false if thing is nil, pool is missing, or the thing does not own `T`.
     */
    template <typename T>
    bool try_destroy(Thing thing) noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();

        if (do_check_thing_nil(thing)) [[unlikely]] {
            return false;
        }

        if (do_check_part_pool(tidx)) [[unlikely]] {
            return false;
        }

        if (!do_check_part(thing, tidx)) [[unlikely]] {
            return false;
        }

        PartPool<T> &pool = m_part_pools[tidx.idx()].cast_ref<PartPool<T>>();
        m_signature_pool.destroy(thing, tidx);
        pool.destroy_unchecked(thing);

        return true;
    }

    /**
     * @brief Destroys part `T` on a thing.
     * @note Returns false if thing is nil.
     * @warning Asserts if the pool is missing or the thing does not have part `T`.
     */
    template <typename T>
    bool destroy(Thing thing) noexcept {
        if (do_check_thing_nil(thing)) [[unlikely]] {
            return false;
        }

        TypeIdx tidx = TypeIdx::from_type<T>();

        FR_ASSERT(!do_check_part_pool(tidx),
                  "cannot destroy part T; T is not registered as a part");
        FR_ASSERT(do_check_part(thing, tidx),
                  "cannot destroy part T if the thing does not own this part");

        PartPool<T> &pool = m_part_pools[tidx.idx()].cast_ref<PartPool<T>>();
        m_signature_pool.destroy(thing, tidx);
        pool.destroy_unchecked(thing);

        return true;
    }

    /**
     * @brief Tries to get the part `T` owned by the thing.
     * @note Returns nullptr if the pool is missing, the thing is dead, or the thing does not own
     * `T`.
     * @note Returns the stub pointer for nil thing if the pool exists.
     */
    template <typename T>
    T *try_get(Thing thing) noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        if (do_check_part_pool(tidx)) [[unlikely]] {
            return nullptr;
        }

        PartPool<T> &pool = m_part_pools[tidx.idx()].cast_ref<PartPool<T>>();
        if (do_check_thing_nil(thing)) [[unlikely]] {
            return &pool.get_stub();
        }

        if (!do_check_thing_alive(thing)) [[unlikely]] {
            return nullptr;
        }

        if (!do_check_part(thing, tidx)) [[unlikely]] {
            return nullptr;
        }

        return pool.get_unchecked(thing);
    }

    /**
     * @brief Returns a reference to the part `T` owned by the thing.
     * @warning Asserts if the thing is dead or does not have a part `T`.
     */
    template <typename T>
    T &get(Thing thing) noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        FR_ASSERT(do_check_thing_alive(thing), "thing has to be alive");
        FR_ASSERT(!do_check_part_pool(tidx), "part pool missing");
        FR_ASSERT(do_check_part(thing, tidx), "thing does not own part");

        return *m_part_pools[tidx.idx()].cast_ref<PartPool<T>>().get_unchecked(thing);
    }

    template <typename T>
    T &get_unchecked(Thing thing) noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        return *m_part_pools[tidx.idx()].cast_ref<PartPool<T>>().get_unchecked(thing);
    }

    /**
     * @brief Creates a query for a set of parts.
     */
    template <typename... Include>
    auto query() noexcept {
        Signature include;
        (include.insert(TypeIdx::from_type<Include>()), ...);
        return fr::Query<Include...>(this, include);
    }

private:
    // -------------------------------------------------------- Internal Helpers

    /**
     * @brief Returns the signature of a thing by its index.
     */
    const Signature &do_signature_by_idx(ThingIdx idx) const noexcept {
        return m_signature_pool.signatures()[idx];
    }

    /**
     * @brief Returns the number of parts in a pool by TypeIdx.
     */
    USize do_part_count_by_tidx(TypeIdx tidx) const noexcept {
        if (do_check_part_pool(tidx)) {
            return 0;
        }

        // Layout of PartPool is same for all T.
        return m_part_pools[tidx.idx()].cast_ref<PartPool<Byte>>().part_count();
    }

    /**
     * @brief Returns the part -> thing mapping slice for a pool by TypeIdx.
     */
    Slice<const Thing> do_part_to_thing_slice_by_tidx(TypeIdx tidx) const noexcept {
        FR_ASSERT(!do_check_part_pool(tidx), "part pool missing");

        // Layout of PartPool is same for all T.
        return m_part_pools[tidx.idx()].cast_ref<PartPool<Byte>>().part_to_thing_slice_with_stub();
    }

    bool do_check_thing_nil(Thing thing) const noexcept {
        return thing.is_nil();
    }

    bool do_check_thing_alive(Thing thing) const noexcept {
        return m_thing_pool.check(thing);
    }

    bool do_check_part_pool(TypeIdx tidx) const noexcept {
        return m_part_pools[tidx.idx()].is_nil();
    }

    template <typename T>
    const PartPool<T> *do_part_pool() const noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        FR_ASSERT(tidx.idx() < MAX_PARTS, "invalid part type index");

        if (do_check_part_pool(tidx)) [[unlikely]] {
            return nullptr;
        }

        return &m_part_pools[tidx.idx()].cast_ref<const PartPool<T>>();
    }

    template <typename T>
    PartPool<T> &do_ensure_part_pool() noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        FR_ASSERT(tidx.idx() < MAX_PARTS, "invalid part type index");

        if (do_check_part_pool(tidx)) [[unlikely]] {
            do_create_part_pool<T>(tidx);
        }

        return m_part_pools[tidx.idx()].cast_ref<PartPool<T>>();
    }

    template <typename T>
    PartPool<T> &do_ensure_part_pool(TypeIdx tidx) noexcept {
        FR_ASSERT(tidx.idx() < MAX_PARTS, "invalid part type index");

        if (do_check_part_pool(tidx)) [[unlikely]] {
            do_create_part_pool<T>(tidx);
        }

        return m_part_pools[tidx.idx()].cast_ref<PartPool<T>>();
    }

    bool do_check_part(Thing thing, TypeIdx tidx) const noexcept {
        return m_signature_pool.owns(thing, tidx);
    }

    template <typename T>
    void do_create_part_pool(TypeIdx tidx) noexcept {
        m_part_pools[tidx.idx()].template emplace<PartPool<T>>(m_alloc);
    }

    // -------------------------------------------------------- Member Variables

    Alloc *m_alloc{nullptr};
    Array<AnyPartPool, MAX_PARTS> m_part_pools{};
    ThingPool m_thing_pool{};
    SignaturePool m_signature_pool{};
};

} // namespace fr::impl

// Resolves a circual dependency between Query and Registry.
#include "fr/data/query.hpp"
