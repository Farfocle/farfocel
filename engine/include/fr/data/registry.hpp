/**
 * @file registry.hpp
 * @author Kiju
 *
 * @brief Registry is the central store for things and parts in Farfocel.
 */

#pragma once

#include <iterator>
#include <utility>

#include "fr/core/inline_any.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/tuple.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/part.hpp"
#include "fr/data/relations.hpp"

namespace fr {
// ======================================================== Forward Declarations

/**
 * @brief Filtering options for a query.
 * @note Use Signature::from_parts<...>() to construct the fields.
 */
struct QueryOptions {
    Signature with{};
    Signature without{};
};
} // namespace fr

namespace fr::impl {

template <bool IsReverse, typename... Include>
class GenericQuery;

template <typename... Include>
class ShallowHierarchyQuery;

template <typename... Include>
class DeepHierarchyQuery;

template <bool IsReverse, typename... Include>
class GenericDepthHierarchyQuery;

// ======================================================================= Registry

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

    /// @brief Returns the allocator used by this registry.
    const Alloc *alloc() const noexcept;

    /// @brief Returns a reference to the thing pool.
    ThingPool &thing_pool_mut() noexcept;

    /// @brief Returns a reference to the signature pool.
    SignaturePool &signature_pool_mut() noexcept;

    /**
     * @brief Returns a pointer to the part pool `T`.
     * @note If the pointer is not yet created returns nullptr.
     */
    template <typename T>
    PartPool<T> *part_pool_mut() noexcept;

    /// @brief Returns true if a part pool for part `T` has been created.
    template <typename T>
    bool check_part_pool() const noexcept;

    // -------------------------------------------------------- Thing Operations

    /// @brief Returns a fresh, non-nil thing.
    Thing handout() noexcept;

    /**
     * @brief Kills a thing, destroys all parts it owns, clears its signature, and recycles its
     * slot.
     * @note If a thing is nil or dead; does nothing.
     */
    void kill(Thing thing) noexcept;

    /// @brief Returns true if the thing is alive. Nil thing is always alive.
    bool is_alive(Thing thing) const noexcept;

    /// @brief Returns true if the thing is dead. Nil thing is never dead.
    bool is_dead(Thing thing) const noexcept;

    // --------------------------------------------------------- Part Operations

    /**
     * @brief Returns true if the thing has part `T`.
     * @note Returns false if pool is missing or thing is dead.
     * @note Returns true for nil thing when the part pool `T` exists.
     */
    template <typename T>
    bool has(Thing thing) const noexcept;

    /**
     * @brief Emplaces or overrides part `T` on a thing.
     * @note If the thing is nil; returns the stub pointer.
     * @note If the thing is dead; return nullptr.
     * @note If the thing does NOT have a part `T`; inserts the new part, returns a pointer to it.
     * @note If the thing does have a part `T`; overrides it, returns a pointer to it.
     */
    template <typename T, typename... Args>
    T *emplace_checked(Thing thing, Args &&...args) noexcept;

    /// @brief Emplaces part `T` on a thing without any checks.
    template <typename T, typename... Args>
    T &emplace_unchecked(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Destroys part `T` on a thing if present.
     * @return false if thing is nil, dead, pool is missing, or thing does not have part `T`, true
     * otherwise.
     */
    template <typename T>
    bool destroy_checked(Thing thing) noexcept;

    /// @brief Destroys part `T` on a thing without any checks.
    template <typename T>
    void destroy_unchecked(Thing thing) noexcept;

    /**
     * @brief Returns a pointer to part `T` owned by the thing, or nullptr if not found.
     * @note If the thing is nil; returns the stub pointer.
     */
    template <typename T>
    T *get_checked(Thing thing) noexcept;

    /// @brief Returns a reference to part `T` on the thing without any checks.
    template <typename T>
    T &get_unchecked(Thing thing) noexcept;

    /// @brief Creates a forward query for things owning all parts in the Include list.
    template <typename... Include>
    auto query(QueryOptions options = {}) noexcept;

    /// @brief Creates a reverse query for things owning all parts in the Include list.
    template <typename... Include>
    auto reverse_query(QueryOptions options = {}) noexcept;

    /// @brief Creates a query over the direct children of a thing.
    template <typename... Include>
    auto shallow_query(Thing thing, QueryOptions options = {}) noexcept;

    /// @brief Creates a depth-first query over all descendants of a thing.
    template <typename... Include>
    auto deep_query(Thing thing, QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a top-down (parents first) forward query.
     * @note This works on any part pools - not only the sorted ones.
     */
    template <typename... Include>
    auto top_down_query(QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a bottom-up (leaves first) forward query.
     * @note This works on any part pools - not only the sorted ones.
     */
    template <typename... Include>
    auto bottom_up_query(QueryOptions options = {}) noexcept;

private:
    // -------------------------------------------------------- Internal Helpers

    bool do_pool_absent(TypeIdx tidx) const noexcept;
    bool do_check_part(Thing thing, TypeIdx tidx) const noexcept;

    template <typename T>
    PartPool<T> &do_ensure_part_pool(TypeIdx tidx) noexcept;

    template <typename T>
    void do_create_part_pool(TypeIdx tidx) noexcept;

    void do_destroy_all_parts(Thing thing) noexcept;

    template <typename T>
    static void do_part_destroy_fn(AnyPartPool &pool, Thing thing) noexcept;

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
inline bool Registry::check_part_pool() const noexcept {
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

inline bool check_signature(const Signature &sig, const Signature &ret,
                            const QueryOptions &opt) noexcept {
    const auto &bits = sig.bitset();
    return (bits & ret.bitset()) == ret.bitset() &&
           (bits & opt.with.bitset()) == opt.with.bitset() && (bits & opt.without.bitset()).none();
}

// ================================================================ GenericQuery

/**
 * @brief Simple query system for the hidden sparse set ECS.
 * @tparam IsReverse If true, iterates in reverse order.
 * @tparam Include List of part types that a thing must have and that are yielded by the iterator.
 */
template <bool IsReverse, typename... Include>
class GenericQuery {
public:
    // ------------------------------------------------------------- Constructor
    GenericQuery(Registry *registry, Signature return_mask, QueryOptions options = {}) noexcept
        : m_registry(registry),
          m_return(return_mask),
          m_options(options) {
        m_iter_tidx = do_find_smallest_pool();
    }

    // ---------------------------------------------------------------- Iterator
    struct Iter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Tuple<Thing, Include &...>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        Iter(Registry *registry, Signature return_mask, QueryOptions options,
             Slice<const Thing> things, USize idx) noexcept
            : m_registry(registry),
              m_return(return_mask),
              m_options(options),
              m_things(things),
              m_idx(idx) {
            do_find_next();
        }

        value_type operator*() const noexcept {
            Thing thing = m_things[m_idx];
            return value_type(thing, m_registry->get_unchecked<Include>(thing)...);
        }

        Iter &operator++() noexcept {
            if constexpr (IsReverse) {
                if (m_idx > 0) {
                    --m_idx;
                }

                do_find_next();
            } else {
                ++m_idx;
                do_find_next();
            }
            return *this;
        }

        bool operator==(const Iter &other) const noexcept {
            return m_idx == other.m_idx;
        }
        bool operator!=(const Iter &other) const noexcept {
            return !(*this == other);
        }

    private:
        void do_find_next() noexcept {
            if constexpr (IsReverse) {
                while (m_idx > 0) {
                    Thing thing = m_things[m_idx];
                    if (!thing.is_nil()) {
                        if (check_signature(m_registry->m_signature_pool.get(thing), m_return,
                                            m_options)) {
                            return;
                        }
                    }

                    --m_idx;
                }
            } else {
                while (m_idx < m_things.size()) {
                    Thing thing = m_things[m_idx];
                    if (!thing.is_nil()) {
                        if (check_signature(m_registry->m_signature_pool.get(thing), m_return,
                                            m_options)) {
                            break;
                        }
                    }

                    ++m_idx;
                }
            }
        }

        Registry *m_registry;
        Signature m_return;
        QueryOptions m_options;
        Slice<const Thing> m_things;
        USize m_idx;
    };

    // -------------------------------------------------------- Iterator Methods
    Iter begin() noexcept {
        if (m_registry->do_pool_absent(m_iter_tidx))
            return Iter(m_registry, m_return, m_options, Slice<const Thing>{}, 0);
        Slice<const Thing> things = m_registry->do_part_to_thing_slice_by_tidx(m_iter_tidx);
        if constexpr (IsReverse) {
            USize last = things.size() > 0 ? things.size() - 1 : 0;
            return Iter(m_registry, m_return, m_options, things, last);
        } else {
            return Iter(m_registry, m_return, m_options, things, 1);
        }
    }

    Iter end() noexcept {
        if (m_registry->do_pool_absent(m_iter_tidx))
            return Iter(m_registry, m_return, m_options, Slice<const Thing>{}, 0);
        Slice<const Thing> things = m_registry->do_part_to_thing_slice_by_tidx(m_iter_tidx);
        if constexpr (IsReverse) {
            return Iter(m_registry, m_return, m_options, things, 0);
        } else {
            return Iter(m_registry, m_return, m_options, things, things.size());
        }
    }

private:
    // -------------------------------------------------------- Internal Helpers
    TypeIdx do_find_smallest_pool() const noexcept {
        TypeIdx tids[] = {TypeIdx::from_type<Include>()...};
        TypeIdx smallest = tids[0];
        USize min = m_registry->do_part_count_by_tidx(smallest);

        for (USize i = 1; i < sizeof...(Include); ++i) {
            USize count = m_registry->do_part_count_by_tidx(tids[i]);
            if (count > 0 && count < min) {
                min = count;
                smallest = tids[i];
            }
        }

        return smallest;
    }

    // ----------------------------------------------------------------- Members
    Registry *m_registry{};
    Signature m_return{};
    QueryOptions m_options{};
    TypeIdx m_iter_tidx{};
};

// ======================================================= ShallowHierarchyQuery

/**
 * @brief Iterates the direct children of a thing by following the Relations sibling chain.
 * @tparam Include List of part types that a child must have and that are yielded.
 */
template <typename... Include>
class ShallowHierarchyQuery {
public:
    // ------------------------------------------------------------- Constructor
    ShallowHierarchyQuery(Registry *registry, Thing root, Signature return_mask,
                          QueryOptions options = {}) noexcept
        : m_registry(registry),
          m_root(root),
          m_return(return_mask),
          m_options(options) {
    }

    // ---------------------------------------------------------------- Iterator
    struct Iter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Tuple<Thing, Include &...>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        Iter(Registry *registry, Signature return_mask, QueryOptions options,
             Thing current) noexcept
            : m_registry(registry),
              m_return(return_mask),
              m_options(options),
              m_current(current) {
            do_find_next();
        }

        value_type operator*() const noexcept {
            return value_type(m_current, m_registry->get_unchecked<Include>(m_current)...);
        }

        Iter &operator++() noexcept {
            m_current = m_registry->get_unchecked<Relations>(m_current).next_sibling;
            do_find_next();
            return *this;
        }

        bool operator==(const Iter &other) const noexcept {
            return m_current == other.m_current;
        }
        bool operator!=(const Iter &other) const noexcept {
            return !(*this == other);
        }

    private:
        void do_find_next() noexcept {
            while (!m_current.is_nil()) {
                if (check_signature(m_registry->signature_pool_mut().get(m_current), m_return,
                                    m_options)) {
                    return;
                }

                m_current = m_registry->get_unchecked<Relations>(m_current).next_sibling;
            }
        }

        Registry *m_registry;
        Signature m_return;
        QueryOptions m_options;
        Thing m_current;
    };

    // -------------------------------------------------------- Iterator Methods
    Iter begin() noexcept {
        const Relations *root_rel = m_registry->get_checked<Relations>(m_root);
        if (root_rel == nullptr) {
            return end();
        }

        return Iter(m_registry, m_return, m_options, root_rel->first_child);
    }

    Iter end() noexcept {
        return Iter(m_registry, m_return, m_options, Thing::nil());
    }

private:
    // ----------------------------------------------------------------- Members
    Registry *m_registry{};
    Thing m_root{};
    Signature m_return{};
    QueryOptions m_options{};
};

// ========================================================== DeepHierarchyQuery

/**
 * @brief Iterates all descendants of a thing in depth-first order.
 * @tparam Include List of part types that children must have and that are yielded.
 * @note Root must own a `Relations` part; if it does not, the query yields nothing.
 */
template <typename... Include>
class DeepHierarchyQuery {
public:
    // ------------------------------------------------------------- Constructor
    DeepHierarchyQuery(Registry *registry, Thing root, Signature return_mask,
                       QueryOptions options = {}) noexcept
        : m_registry(registry),
          m_root(root),
          m_return(return_mask),
          m_options(options) {
    }

    // ---------------------------------------------------------------- Iterator
    struct Iter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Tuple<Thing, Include &...>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        Iter(Registry *registry, Signature return_mask, QueryOptions options, Thing root,
             Thing current) noexcept
            : m_registry(registry),
              m_return(return_mask),
              m_options(options),
              m_root(root),
              m_current(current) {
            do_find_next();
        }

        value_type operator*() const noexcept {
            return value_type(m_current, m_registry->get_unchecked<Include>(m_current)...);
        }

        Iter &operator++() noexcept {
            do_advance();
            do_find_next();
            return *this;
        }

        bool operator==(const Iter &other) const noexcept {
            return m_current == other.m_current;
        }
        bool operator!=(const Iter &other) const noexcept {
            return !(*this == other);
        }

    private:
        void do_advance() noexcept {
            const Relations &cur_rel = m_registry->get_unchecked<Relations>(m_current);

            if (!cur_rel.first_child.is_nil()) {
                m_current = cur_rel.first_child;
                return;
            }

            if (!cur_rel.next_sibling.is_nil()) {
                m_current = cur_rel.next_sibling;
                return;
            }

            Thing up = cur_rel.parent;
            while (!up.is_nil() && up != m_root) {
                const Relations &up_rel = m_registry->get_unchecked<Relations>(up);
                if (!up_rel.next_sibling.is_nil()) {
                    m_current = up_rel.next_sibling;
                    return;
                }

                up = up_rel.parent;
            }

            m_current = Thing::nil();
        }

        void do_find_next() noexcept {
            while (!m_current.is_nil()) {
                if (check_signature(m_registry->signature_pool_mut().get(m_current), m_return,
                                    m_options)) {
                    return;
                }

                do_advance();
            }
        }

        Registry *m_registry;
        Signature m_return;
        QueryOptions m_options;
        Thing m_root;
        Thing m_current;
    };

    // ----------------------------------------------------------------- Methods
    Iter begin() noexcept {
        const Relations *root_rel = m_registry->get_checked<Relations>(m_root);
        if (root_rel == nullptr) {
            return end();
        }

        return Iter(m_registry, m_return, m_options, m_root, root_rel->first_child);
    }

    Iter end() noexcept {
        return Iter(m_registry, m_return, m_options, m_root, Thing::nil());
    }

private:
    // -------------------------------------------------------- Member Variables
    Registry *m_registry{};
    Thing m_root{};
    Signature m_return{};
    QueryOptions m_options{};
};

// ============================================= GenericDepthHierarchyQuery

/**
 * @brief Iterates a part pool assumed to be pre-sorted by hierarchy depth.
 * @tparam IsReverse If false, iterates top-down (parents first); if true, bottom-up (leaves first).
 * @tparam Include Part types that a thing must have and that are yielded.
 */
template <bool IsReverse, typename... Include>
class GenericDepthHierarchyQuery {
public:
    // ------------------------------------------------------------- Constructor
    GenericDepthHierarchyQuery(Registry *registry, Signature return_mask,
                               QueryOptions options = {}) noexcept
        : m_registry(registry),
          m_return(return_mask),
          m_options(options) {
        TypeIdx tids[] = {TypeIdx::from_type<Include>()...};
        m_iter_tidx = tids[0];
    }

    // ---------------------------------------------------------------- Iterator
    struct Iter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Tuple<Thing, Include &...>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        Iter(Registry *registry, Signature return_mask, QueryOptions options,
             Slice<const Thing> things, USize idx) noexcept
            : m_registry(registry),
              m_return(return_mask),
              m_options(options),
              m_things(things),
              m_idx(idx) {
            do_find_next();
        }

        value_type operator*() const noexcept {
            Thing thing = m_things[m_idx];
            return value_type(thing, m_registry->get_unchecked<Include>(thing)...);
        }

        Iter &operator++() noexcept {
            if constexpr (IsReverse) {
                if (m_idx > 0)
                    --m_idx;
                do_find_next();
            } else {
                ++m_idx;
                do_find_next();
            }
            return *this;
        }

        bool operator==(const Iter &other) const noexcept {
            return m_idx == other.m_idx;
        }
        bool operator!=(const Iter &other) const noexcept {
            return !(*this == other);
        }

    private:
        void do_find_next() noexcept {
            if constexpr (IsReverse) {
                while (m_idx > 0) {
                    Thing thing = m_things[m_idx];
                    if (!thing.is_nil()) {
                        if (check_signature(m_registry->m_signature_pool.get(thing), m_return,
                                            m_options))
                            return;
                    }
                    --m_idx;
                }
            } else {
                while (m_idx < m_things.size()) {
                    Thing thing = m_things[m_idx];
                    if (!thing.is_nil()) {
                        if (check_signature(m_registry->m_signature_pool.get(thing), m_return,
                                            m_options))
                            break;
                    }
                    ++m_idx;
                }
            }
        }

        Registry *m_registry;
        Signature m_return;
        QueryOptions m_options;
        Slice<const Thing> m_things;
        USize m_idx;
    };

    // -------------------------------------------------------- Iterator Methods
    Iter begin() noexcept {
        if (m_registry->do_pool_absent(m_iter_tidx))
            return Iter(m_registry, m_return, m_options, Slice<const Thing>{}, 0);
        Slice<const Thing> things = m_registry->do_part_to_thing_slice_by_tidx(m_iter_tidx);
        if constexpr (IsReverse) {
            USize last = things.size() > 0 ? things.size() - 1 : 0;
            return Iter(m_registry, m_return, m_options, things, last);
        } else {
            return Iter(m_registry, m_return, m_options, things, 1);
        }
    }

    Iter end() noexcept {
        if (m_registry->do_pool_absent(m_iter_tidx))
            return Iter(m_registry, m_return, m_options, Slice<const Thing>{}, 0);
        Slice<const Thing> things = m_registry->do_part_to_thing_slice_by_tidx(m_iter_tidx);
        if constexpr (IsReverse) {
            return Iter(m_registry, m_return, m_options, things, 0);
        } else {
            return Iter(m_registry, m_return, m_options, things, things.size());
        }
    }

private:
    // ----------------------------------------------------------------- Members
    Registry *m_registry{};
    Signature m_return{};
    QueryOptions m_options{};
    TypeIdx m_iter_tidx{};
};

// ========================= Registry Query Method Implementations (post-definition)

template <typename... Include>
inline auto Registry::query(QueryOptions options) noexcept {
    return GenericQuery<false, Include...>(this, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto Registry::reverse_query(QueryOptions options) noexcept {
    return GenericQuery<true, Include...>(this, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto Registry::shallow_query(Thing thing, QueryOptions options) noexcept {
    return ShallowHierarchyQuery<Include...>(this, thing, Signature::from_parts<Include...>(),
                                             options);
}

template <typename... Include>
inline auto Registry::deep_query(Thing thing, QueryOptions options) noexcept {
    return DeepHierarchyQuery<Include...>(this, thing, Signature::from_parts<Include...>(),
                                          options);
}

template <typename... Include>
inline auto Registry::top_down_query(QueryOptions options) noexcept {
    return GenericDepthHierarchyQuery<false, Include...>(this, Signature::from_parts<Include...>(),
                                                         options);
}

template <typename... Include>
inline auto Registry::bottom_up_query(QueryOptions options) noexcept {
    return GenericDepthHierarchyQuery<true, Include...>(this, Signature::from_parts<Include...>(),
                                                        options);
}

} // namespace fr::impl

// ================================================================ API Typedefs

namespace fr {

template <typename... Include>
using Query = impl::GenericQuery<false, Include...>;

template <typename... Include>
using ReverseQuery = impl::GenericQuery<true, Include...>;

template <typename... Include>
using ShallowQuery = impl::ShallowHierarchyQuery<Include...>;

template <typename... Include>
using DeepQuery = impl::DeepHierarchyQuery<Include...>;

template <typename... Include>
using TopDownQuery = impl::GenericDepthHierarchyQuery<false, Include...>;

template <typename... Include>
using BottomUpQuery = impl::GenericDepthHierarchyQuery<true, Include...>;

} // namespace fr
