/**
 * @file query.hpp
 * @author Kiju
 *
 * @brief Simple query system for the hidden sparse set ECS.
 */

#pragma once

#include "fr/core/meta.hpp"
#include "fr/core/tuple.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/thing.hpp"

namespace fr {

namespace impl {

/**
 * @brief Simple query system for the hidden sparse set ECS.
 * @tparam IsReverse If true, iterates in reverse order over the smallest pool.
 * @tparam Include List of part types that a thing must own and that are yielded by the iterator.
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
                        const Signature &signature = m_registry->m_signature_pool.get(thing);
                        if (do_check_signature(signature)) {
                            return;
                        }
                    }
                    --m_idx;
                }
            } else {
                while (m_idx < m_things.size()) {
                    Thing thing = m_things[m_idx];
                    if (!thing.is_nil()) {
                        const Signature &signature = m_registry->m_signature_pool.get(thing);
                        if (do_check_signature(signature)) {
                            break;
                        }
                    }
                    ++m_idx;
                }
            }
        }

        bool do_check_signature(const Signature &signature) const noexcept {
            const auto &bits = signature.bitset();
            const auto &return_bits = m_return.bitset();
            const auto &with_bits = m_options.with.bitset();
            const auto &without_bits = m_options.without.bitset();

            return (bits & return_bits) == return_bits && (bits & with_bits) == with_bits &&
                   (bits & without_bits).none();
        }

        Registry *m_registry;
        Signature m_return;
        QueryOptions m_options;
        Slice<const Thing> m_things;
        USize m_idx;
    };

    // ----------------------------------------------------------------- Methods
    Iter begin() noexcept {
        if (m_registry->do_pool_absent(m_iter_tidx)) {
            return Iter(m_registry, m_return, m_options, Slice<const Thing>{}, 0);
        }

        Slice<const Thing> part_to_thing = m_registry->do_part_to_thing_slice_by_tidx(m_iter_tidx);
        if constexpr (IsReverse) {
            USize last = part_to_thing.size() > 0 ? part_to_thing.size() - 1 : 0;
            return Iter(m_registry, m_return, m_options, part_to_thing, last);
        } else {
            return Iter(m_registry, m_return, m_options, part_to_thing, 1);
        }
    }

    Iter end() noexcept {
        if (m_registry->do_pool_absent(m_iter_tidx)) {
            return Iter(m_registry, m_return, m_options, Slice<const Thing>{}, 0);
        }

        Slice<const Thing> part_to_thing = m_registry->do_part_to_thing_slice_by_tidx(m_iter_tidx);
        if constexpr (IsReverse) {
            return Iter(m_registry, m_return, m_options, part_to_thing, 0);
        } else {
            return Iter(m_registry, m_return, m_options, part_to_thing, part_to_thing.size());
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

    // -------------------------------------------------------- Member Variables
    Registry *m_registry;
    Signature m_return{};
    QueryOptions m_options{};
    TypeIdx m_iter_tidx;
};

} // namespace impl

template <typename... Include>
using Query = impl::GenericQuery<false, Include...>;

template <typename... Include>
using ReverseQuery = impl::GenericQuery<true, Include...>;

} // namespace fr
