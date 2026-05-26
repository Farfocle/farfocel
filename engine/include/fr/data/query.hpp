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

/**
 * @brief Simple query system for the hidden sparse set ECS.
 * @tparam Include List of part types that a thing must own.
 */
template <typename... Include>
class Query {
public:
    // ------------------------------------------------------------- Constructor
    Query(impl::Registry *registry, Signature include_mask) noexcept
        : m_registry(registry),
          m_include(include_mask) {
        m_iter_tidx = do_find_smallest_pool();
    }

    // ---------------------------------------------------------------- Iterator
    struct Iter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Tuple<Thing, Include &...>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        Iter(impl::Registry *registry, Signature include, Signature exclude,
             Slice<const Thing> things, USize idx) noexcept
            : m_registry(registry),
              m_include(include),
              m_exclude(exclude),
              m_things(things),
              m_idx(idx) {
            do_find_next();
        }

        value_type operator*() const noexcept {
            Thing thing = m_things[m_idx];
            return value_type(thing, m_registry->get_unchecked<Include>(thing)...);
        }

        Iter &operator++() noexcept {
            ++m_idx;
            do_find_next();
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

        bool do_check_signature(const Signature &signature) const noexcept {
            const auto &bits = signature.bitset();
            const auto &include_bits = m_include.bitset();
            const auto &exclude_bits = m_exclude.bitset();

            return (bits & include_bits) == include_bits && (bits & exclude_bits).none();
        }

        impl::Registry *m_registry;
        Signature m_include;
        Signature m_exclude;
        Slice<const Thing> m_things;
        USize m_idx;
    };

    // ----------------------------------------------------------------- Methods
    /**
     * @brief Adds exclusion filters to the query.
     * @tparam Exclude List of part types that a thing must NOT own.
     */
    template <typename... Exclude>
    Query &without() noexcept {
        (m_exclude.insert(TypeIdx::from_type<Exclude>()), ...);
        return *this;
    }

    Iter begin() noexcept {
        Slice<const Thing> part_to_thing = m_registry->do_part_to_thing_slice_by_tidx(m_iter_tidx);
        return Iter(m_registry, m_include, m_exclude, part_to_thing, 1);
    }

    Iter end() noexcept {
        Slice<const Thing> part_to_thing = m_registry->do_part_to_thing_slice_by_tidx(m_iter_tidx);
        return Iter(m_registry, m_include, m_exclude, part_to_thing, part_to_thing.size());
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
    impl::Registry *m_registry;
    Signature m_include{};
    Signature m_exclude{};
    TypeIdx m_iter_tidx;
};

} // namespace fr
