/**
 * @file registry.hpp
 * @author Kiju
 *
 * @brief Registry is the central store for things and parts in Farfocel.
 *
 * @par Responsibilities
 *   - Manages thing lifetimes (handout / kill).
 *   - Manages per-thing signatures (which parts a thing owns).
 *   - Manages per-type PartPools and routes part operations to them.
 *   - On kill, destroys all parts owned by the thing before invalidating it.
 *
 * @par Checked vs. Unchecked
 *   - `*_checked`   : handles nil things, dead things, and missing pools/parts gracefully.
 *   - `*_unchecked` : caller guarantees all preconditions; no defensive checks.
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

namespace fr::impl {

template <bool IsReverse, typename... Include>
class GenericQuery;

template <typename... Include>
class ShallowHierarchyQuery;

template <typename... Include>
class DeepHierarchyQuery;

template <bool IsReverse, typename... Include>
class GenericDepthHierarchyQuery;

class Registry {
    template <bool IsReverse, typename... Include>
    friend class GenericQuery;

    template <typename... Include>
    friend class ShallowHierarchyQuery;

    template <typename... Include>
    friend class DeepHierarchyQuery;

    template <bool IsReverse, typename... Include>
    friend class GenericDepthHierarchyQuery;

public:
    using AnyPartPool = InlineAny<sizeof(PartPool<Byte>), alignof(PartPool<Byte>)>;

    /**
     * @brief Type-erased function that destroys one thing's part entry inside an AnyPartPool.
     * Registered once per part type when its pool is first created.
     */
    using PartDestroyFn = void (*)(AnyPartPool &, Thing) noexcept;

    // --------------------------------------------- Constructors and Destructor

    Registry() noexcept;
    explicit Registry(Alloc *alloc) noexcept;

    Registry(const Registry &) = delete;
    Registry(Registry &&) = delete;
    Registry &operator=(const Registry &) = delete;
    Registry &operator=(Registry &&) = delete;

    // ------------------------------------------------- Internal Storage Access

    /**
     * @brief Returns the allocator used by this registry.
     */
    const Alloc *alloc() const noexcept;

    /**
     * @brief Returns the thing pool.
     */
    ThingPool &thing_pool_mut() noexcept;

    /**
     * @brief Returns the signature pool.
     */
    SignaturePool &signature_pool_mut() noexcept;

    /**
     * @brief Returns a pointer to the PartPool for T, or nullptr if not yet created.
     */
    template <typename T>
    PartPool<T> *part_pool_mut() noexcept;

    /**
     * @brief Returns true if a PartPool for T has been created.
     */
    template <typename T>
    bool has_part_pool() const noexcept;

    // -------------------------------------------------------- Thing Operations

    /**
     * @brief Returns a fresh, non-nil thing.
     */
    Thing handout() noexcept;

    /**
     * @brief Kills a thing.
     * Destroys all parts it owns, clears its signature, and recycles its slot.
     * @note Does nothing for nil or already-dead things.
     */
    void kill(Thing thing) noexcept;

    /**
     * @brief Returns true if the thing is alive. Nil thing is always alive.
     */
    bool is_alive(Thing thing) const noexcept;

    /**
     * @brief Returns true if the thing is dead. Nil thing is never dead.
     */
    bool is_dead(Thing thing) const noexcept;

    // --------------------------------------------------------- Part Operations

    /**
     * @brief Returns true if the thing owns part T.
     * @note Returns false if pool is missing or thing is dead.
     * @note Returns true for nil thing when the pool exists (nil -> stub).
     */
    template <typename T>
    bool has(Thing thing) const noexcept;

    /**
     * @brief Inserts or overrides part T on a thing.
     * - Nil thing  : returns the stub pointer.
     * - Dead thing : returns nullptr.
     * - Alive, no T: inserts T, returns pointer to it.
     * - Alive, has T: overrides T in-place, returns pointer to it.
     */
    template <typename T, typename... Args>
    T *emplace_checked(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Inserts part T on a thing without any checks.
     * @pre Caller must ensure: thing is alive (not nil, not dead), thing does NOT yet own T.
     */
    template <typename T, typename... Args>
    T &emplace_unchecked(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Destroys part T on a thing if present.
     * @return false if thing is nil, dead, pool is missing, or thing does not own T.
     */
    template <typename T>
    bool destroy_checked(Thing thing) noexcept;

    /**
     * @brief Destroys part T on a thing without any checks.
     * @pre Caller must ensure: thing is alive, thing DOES own T, pool exists.
     */
    template <typename T>
    void destroy_unchecked(Thing thing) noexcept;

    /**
     * @brief Returns a pointer to part T owned by the thing, or nullptr if not found.
     * @note Returns stub pointer for nil thing when the pool exists.
     */
    template <typename T>
    T *get_checked(Thing thing) noexcept;

    /**
     * @brief Returns a reference to part T owned by the thing.
     * @pre Caller must ensure: thing is alive, thing owns T, pool exists.
     */
    template <typename T>
    T &get_unchecked(Thing thing) noexcept;

    /**
     * @brief Creates a query for things owning all parts in the Include list.
     */
    template <typename... Include>
    auto query(QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a reverse query for things owning all parts in the Include list.
     */
    template <typename... Include>
    auto reverse_query(QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a top-down (parents first) query over the first Include type's sorted pool.
     * @note Call World::sort_by_hierarchy_depth<T>() on the first Include type before using.
     */
    template <typename... Include>
    auto top_down_query(QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a bottom-up (leaves first) query over the first Include type's sorted pool.
     * @note Call World::sort_by_hierarchy_depth<T>() on the first Include type before using.
     */
    template <typename... Include>
    auto bottom_up_query(QueryOptions options = {}) noexcept;

private:
    // -------------------------------------------------------- Internal Helpers

    /**
     * @brief Returns true if no pool has been created for this type index (pool is absent).
     */
    bool do_pool_absent(TypeIdx tidx) const noexcept;

    /**
     * @brief Returns true if the thing's signature contains the given part type.
     */
    bool do_check_part(Thing thing, TypeIdx tidx) const noexcept;

    /**
     * @brief Returns the pool for T, creating it if it does not yet exist.
     */
    template <typename T>
    PartPool<T> &do_ensure_part_pool(TypeIdx tidx) noexcept;

    /**
     * @brief Creates a new PartPool for T at the given type index slot.
     * Also registers the type-erased destroy function used by kill().
     */
    template <typename T>
    void do_create_part_pool(TypeIdx tidx) noexcept;

    /**
     * @brief Destroys all parts owned by the thing by iterating its signature.
     * Called by kill() before invalidating the thing.
     */
    void do_destroy_all_parts(Thing thing) noexcept;

    /**
     * @brief Type-erased destroy function stored in m_part_destroy_fns.
     * Casts the pool to PartPool<T> and calls destroy_unchecked.
     */
    template <typename T>
    static void do_part_destroy_fn(AnyPartPool &pool, Thing thing) noexcept;

    // Used by Query.
    USize do_part_count_by_tidx(TypeIdx tidx) const noexcept;
    Slice<const Thing> do_part_to_thing_slice_by_tidx(TypeIdx tidx) const noexcept;

    // -------------------------------------------------------- Member Variables
    Alloc *m_alloc{nullptr};
    Array<AnyPartPool, MAX_PARTS> m_part_pools{};
    Array<PartDestroyFn, MAX_PARTS> m_part_destroy_fns{};
    ThingPool m_thing_pool{};
    SignaturePool m_signature_pool{};
};

// ============================================= Registry Method Implementations

inline Registry::Registry() noexcept
    : Registry(get_ambient_ctx().alloc) {
}

inline Registry::Registry(Alloc *alloc) noexcept
    : m_alloc(alloc),
      m_thing_pool(alloc),
      m_signature_pool(alloc) {
}

inline const Alloc *Registry::alloc() const noexcept {
    return m_alloc;
}

inline ThingPool &Registry::thing_pool_mut() noexcept {
    return m_thing_pool;
}

inline SignaturePool &Registry::signature_pool_mut() noexcept {
    return m_signature_pool;
}

template <typename T>
inline PartPool<T> *Registry::part_pool_mut() noexcept {
    TypeIdx tidx = TypeIdx::from_type<T>();
    if (do_pool_absent(tidx)) {
        return nullptr;
    }

    return &m_part_pools[tidx.idx()].cast_ref<PartPool<T>>();
}

template <typename T>
inline bool Registry::has_part_pool() const noexcept {
    return !do_pool_absent(TypeIdx::from_type<T>());
}

inline Thing Registry::handout() noexcept {
    return m_thing_pool.handout();
}

inline void Registry::kill(Thing thing) noexcept {
    if (thing.is_nil()) [[unlikely]] {
        return;
    }
    if (!m_thing_pool.check(thing)) [[unlikely]] {
        return;
    }

    do_destroy_all_parts(thing);
    m_signature_pool.destroy_all(thing);
    m_thing_pool.kill_alive(thing);
}

inline bool Registry::is_alive(Thing thing) const noexcept {
    if (thing.is_nil()) [[unlikely]] {
        return true;
    }
    return m_thing_pool.check(thing);
}

inline bool Registry::is_dead(Thing thing) const noexcept {
    return !is_alive(thing);
}

template <typename T>
inline bool Registry::has(Thing thing) const noexcept {
    TypeIdx tidx = TypeIdx::from_type<T>();
    if (do_pool_absent(tidx)) [[unlikely]] {
        return false;
    }
    if (thing.is_nil()) [[unlikely]] {
        return true;
    }
    if (!m_thing_pool.check(thing)) [[unlikely]] {
        return false;
    }
    return do_check_part(thing, tidx);
}

template <typename T, typename... Args>
inline T *Registry::emplace_checked(Thing thing, Args &&...args) noexcept {
    TypeIdx tidx = TypeIdx::from_type<T>();
    PartPool<T> &pool = do_ensure_part_pool<T>(tidx);

    if (thing.is_nil()) [[unlikely]] {
        return &pool.get_stub();
    }
    if (!m_thing_pool.check(thing)) [[unlikely]] {
        return nullptr;
    }

    if (do_check_part(thing, tidx)) {
        return &pool.override_unchecked(thing, std::forward<Args>(args)...);
    }

    m_signature_pool.insert(thing, tidx);
    return &pool.emplace_unchecked(thing, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline T &Registry::emplace_unchecked(Thing thing, Args &&...args) noexcept {
    TypeIdx tidx = TypeIdx::from_type<T>();
    PartPool<T> &pool = do_ensure_part_pool<T>(tidx);
    m_signature_pool.insert(thing, tidx);
    return pool.emplace_unchecked(thing, std::forward<Args>(args)...);
}

template <typename T>
inline bool Registry::destroy_checked(Thing thing) noexcept {
    if (thing.is_nil()) [[unlikely]] {
        return false;
    }
    TypeIdx tidx = TypeIdx::from_type<T>();
    if (do_pool_absent(tidx)) [[unlikely]] {
        return false;
    }
    if (!m_thing_pool.check(thing)) [[unlikely]] {
        return false;
    }
    if (!do_check_part(thing, tidx)) [[unlikely]] {
        return false;
    }

    m_signature_pool.destroy(thing, tidx);
    m_part_pools[tidx.idx()].cast_ref<PartPool<T>>().destroy_unchecked(thing);
    return true;
}

template <typename T>
inline void Registry::destroy_unchecked(Thing thing) noexcept {
    TypeIdx tidx = TypeIdx::from_type<T>();
    m_signature_pool.destroy(thing, tidx);
    m_part_pools[tidx.idx()].cast_ref<PartPool<T>>().destroy_unchecked(thing);
}

template <typename T>
inline T *Registry::get_checked(Thing thing) noexcept {
    TypeIdx tidx = TypeIdx::from_type<T>();
    if (do_pool_absent(tidx)) [[unlikely]] {
        return nullptr;
    }
    PartPool<T> &pool = m_part_pools[tidx.idx()].cast_ref<PartPool<T>>();
    if (thing.is_nil()) [[unlikely]] {
        return &pool.get_stub();
    }
    if (!m_thing_pool.check(thing)) [[unlikely]] {
        return nullptr;
    }
    if (!do_check_part(thing, tidx)) [[unlikely]] {
        return nullptr;
    }
    return pool.get_unchecked(thing);
}

template <typename T>
inline T &Registry::get_unchecked(Thing thing) noexcept {
    TypeIdx tidx = TypeIdx::from_type<T>();
    FR_ASSERT(!do_pool_absent(tidx), "part pool missing");
    FR_ASSERT(m_thing_pool.check(thing), "thing must be alive");
    FR_ASSERT(do_check_part(thing, tidx), "thing does not own part T");
    return *m_part_pools[tidx.idx()].cast_ref<PartPool<T>>().get_unchecked(thing);
}

template <typename... Include>
inline auto Registry::query(QueryOptions options) noexcept {
    return GenericQuery<false, Include...>(this, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto Registry::reverse_query(QueryOptions options) noexcept {
    return GenericQuery<true, Include...>(this, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto Registry::top_down_query(QueryOptions options) noexcept {
    return GenericDepthHierarchyQuery<false, Include...>(this, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto Registry::bottom_up_query(QueryOptions options) noexcept {
    return GenericDepthHierarchyQuery<true, Include...>(this, Signature::from_parts<Include...>(), options);
}

// ------------------------------------------------------------ Internal Helpers

inline bool Registry::do_pool_absent(TypeIdx tidx) const noexcept {
    return m_part_pools[tidx.idx()].is_nil();
}

inline bool Registry::do_check_part(Thing thing, TypeIdx tidx) const noexcept {
    return m_signature_pool.owns(thing, tidx);
}

template <typename T>
inline PartPool<T> &Registry::do_ensure_part_pool(TypeIdx tidx) noexcept {
    FR_ASSERT(tidx.idx() < MAX_PARTS, "part type index out of range");
    if (do_pool_absent(tidx)) [[unlikely]] {
        do_create_part_pool<T>(tidx);
    }
    return m_part_pools[tidx.idx()].cast_ref<PartPool<T>>();
}

template <typename T>
inline void Registry::do_create_part_pool(TypeIdx tidx) noexcept {
    m_part_pools[tidx.idx()].template emplace<PartPool<T>>(m_alloc);
    m_part_destroy_fns[tidx.idx()] = &Registry::do_part_destroy_fn<T>;
}

inline void Registry::do_destroy_all_parts(Thing thing) noexcept {
    const Signature &sig = m_signature_pool.get(thing);
    for (USize i = 0; i < MAX_PARTS; ++i) {
        if (m_part_destroy_fns[i] != nullptr && sig.has(TypeIdx::from_idx(i))) {
            m_part_destroy_fns[i](m_part_pools[i], thing);
        }
    }
}

template <typename T>
inline void Registry::do_part_destroy_fn(AnyPartPool &pool, Thing thing) noexcept {
    pool.cast_ref<PartPool<T>>().destroy_unchecked(thing);
}

inline USize Registry::do_part_count_by_tidx(TypeIdx tidx) const noexcept {
    if (do_pool_absent(tidx)) {
        return 0;
    }
    return m_part_pools[tidx.idx()].cast_ref<PartPool<Byte>>().part_count();
}

inline Slice<const Thing> Registry::do_part_to_thing_slice_by_tidx(TypeIdx tidx) const noexcept {
    FR_ASSERT(!do_pool_absent(tidx), "part pool missing");
    return m_part_pools[tidx.idx()].cast_ref<PartPool<Byte>>().part_to_thing_with_stub();
}

} // namespace fr::impl

// Resolves a circular dependency between Query and Registry.
#include "fr/data/query.hpp"
