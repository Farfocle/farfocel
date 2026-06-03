/**
 * @file query.hpp
 * @author Kiju
 *
 * @brief Simple query system for the hidden sparse set ECS.
 */

#pragma once

#include <iterator>

#include "fr/core/meta.hpp"
#include "fr/core/tuple.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/relations.hpp"
#include "fr/data/thing.hpp"

namespace fr {
namespace impl {

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

} // namespace impl

// ================================================================ API Typedefs

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
