/**
 * @file query.hpp
 * @author Kiju
 *
 * @brief Query system for the hidden sparse set ECS.
 */

#pragma once

#include "fr/core/tuple.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/typeidx.hpp"

namespace fr {

/**
 * @brief Query provides a way to iterate over parts.
 * @tparam Include List of part types that a thing must own.
 */
template <typename... Include>
class Query {
public:
    Query(impl::Registry *registry, Signature include_mask) noexcept
        : m_registry(registry),
          m_include_mask(include_mask) {
        m_iterator_pool_idx = do_find_smallest_pool();
    }

    /**
     * @brief Adds exclusion filters to the query.
     * @tparam Exclude List of part types that a thing must NOT own.
     */
    template <typename... Exclude>
    Query &without() noexcept {
        (m_exclude_mask.attach(impl::DataTypeIdxGen::gen<Exclude>()), ...);
        return *this;
    }

    /**
     * @brief Iterator for matching things.
     */
    struct Iter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Tuple<Include &...>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        Iter(impl::Registry *registry, Signature include, Signature exclude,
             Slice<const ThingIdx> things, USize idx) noexcept
            : m_registry(registry),
              m_include(include),
              m_exclude(exclude),
              m_things(things),
              m_idx(idx) {
            do_find_next();
        }

        value_type operator*() const noexcept {
            ThingIdx tidx = m_things[m_idx];
            return value_type(m_registry->get<Include>(Thing(tidx, 0))...);
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
                ThingIdx tidx = m_things[m_idx];

                if (tidx != 0) {
                    const Signature &sig = m_registry->do_get_signature(tidx);
                    if (do_check_signature(sig)) {
                        break;
                    }
                }

                ++m_idx;
            }
        }

        bool do_check_signature(const Signature &sig) const noexcept {
            const auto &bits = sig.bitset();
            const auto &inc = m_include.bitset();
            const auto &exc = m_exclude.bitset();

            return (bits & inc) == inc && (bits & exc).none();
        }

        impl::Registry *m_registry;
        Signature m_include;
        Signature m_exclude;
        Slice<const ThingIdx> m_things;
        USize m_idx;
    };

    Iter begin() noexcept {
        Slice<const ThingIdx> things = m_registry->do_get_part_to_thing_slice(m_iterator_pool_idx);
        return Iter(m_registry, m_include_mask, m_exclude_mask, things, 1);
    }

    Iter end() noexcept {
        Slice<const ThingIdx> things = m_registry->do_get_part_to_thing_slice(m_iterator_pool_idx);
        return Iter(m_registry, m_include_mask, m_exclude_mask, things, things.size());
    }

private:
    TypeIdx do_find_smallest_pool() const noexcept {
        TypeIdx ids[] = {impl::DataTypeIdxGen::gen<Include>()...};
        TypeIdx smallest = ids[0];
        USize min_count = m_registry->do_get_part_count(smallest);

        for (USize i = 1; i < sizeof...(Include); ++i) {
            USize count = m_registry->do_get_part_count(ids[i]);

            if (count > 0 && count < min_count) {
                min_count = count;
                smallest = ids[i];
            }
        }

        return smallest;
    }

    impl::Registry *m_registry;
    Signature m_include_mask{};
    Signature m_exclude_mask{};
    TypeIdx m_iterator_pool_idx;
};

} // namespace fr
