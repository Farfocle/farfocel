/**
 * @file spatial_hash_grid.hpp
 * @author Kiju
 *
 * @brief Special hash grid for collision detection.
 */

#pragma once

#include "fr/core/algo.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {

/// @brief A spatial hash grid for collision detection.
class SpatialHashGrid {
public:
    // ---------------------------------------------------------------- Typedefs
    using Hash = U32;

    /// @brief A single entry in the hash grid.
    struct Entry {
        Thing thing;
        Hash hash;
    };

    /// @brief Options for configuring the spatial hash grid.
    struct Options {
        /// @brief The size of the cell in the hash grid.
        F32 cell_size{1.0};

        /// @brief The size of the hash table. Large prime number to minimize collisions.
        U32 table_size{100003};

        /// @brief The amont of entries to preallocate in the hash table.
        U32 preallocate_size{1000};
    };

private:
    // ----------------------------------------------------------------- Members
    DynamicArray<Entry> m_entries;
    Options m_options;

public:
    // ------------------------------------------------------------ Constructors

    SpatialHashGrid()
        : SpatialHashGrid(get_ambient_ctx().alloc, {}) {};

    explicit SpatialHashGrid(Options options) noexcept
        : SpatialHashGrid(get_ambient_ctx().alloc, options) {};

    SpatialHashGrid(Alloc *alloc, Options options) noexcept
        : m_entries(alloc),
          m_options(options) {
        m_entries.reserve(m_options.preallocate_size);
    };

    SpatialHashGrid(const SpatialHashGrid &) noexcept = default;
    SpatialHashGrid(SpatialHashGrid &&) noexcept = default;
    SpatialHashGrid &operator=(const SpatialHashGrid &) noexcept = default;
    SpatialHashGrid &operator=(SpatialHashGrid &&) noexcept = default;

    ~SpatialHashGrid() noexcept = default;

    // -------------------------------------------------------------------- API

    /// @brief Pushes a thing into the grid at the given position.
    void push(Thing thing, S32 x, S32 y, S32 z) noexcept {
        m_entries.push_back(
            Entry{.thing = thing, .hash = do_hash_cell(x, y, z, m_options.table_size)});
    }

    /// @brief Clears the grid, removing all entries.
    void clear() noexcept {
        m_entries.clear();
    }

    /// @brief Sorts the entries by their hash value.
    void sort() noexcept {
        radix_sort_key(m_entries.slice_mut(), [](const Entry &entry) { return entry.hash; });
    }

    /// @brief Returns a const reference to the internal entries array.
    const DynamicArray<Entry> &entries() const noexcept {
        return m_entries;
    }

    /// @brief Returns the options this grid was configured with.
    const Options &options() const noexcept {
        return m_options;
    }

private:
    // --------------------------------------------------------------- Internals

    [[nodiscard]] static constexpr Hash do_hash_cell(S32 x, S32 y, S32 z, U32 table_size) noexcept {
        constexpr U32 p1 = 73856093;
        constexpr U32 p2 = 19349663;
        constexpr U32 p3 = 83492791;

        // Cast to unsigned to avoid undefined behavior on negative coordinates
        Hash h =
            (static_cast<Hash>(x) * p1) ^ (static_cast<Hash>(y) * p2) ^ (static_cast<Hash>(z) * p3);

        return h % table_size;
    };
};
} // namespace fr::impl
