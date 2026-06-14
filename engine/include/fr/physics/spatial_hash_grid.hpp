/**
 * @file spatial_hash_grid.hpp
 * @author Kiju
 *
 * @brief Spatial hash grid for broad-phase collision detection.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {

/// @brief A spatial hash grid for collision detection.
class SpatialHashGrid {
public:
    using Hash = U32;

    /// @brief A single entry in the hash grid.
    struct Entry {
        Thing thing;
        Hash hash;
    };

    /// @brief Options for configuring the spatial hash grid.
    struct Options {
        /// @brief The size of each cell.
        F32 cell_size{1.0f};

        /// @brief The size of the hash table. Large prime number to minimize collisions.
        U32 table_size{100003};

        /// @brief The number of entries to preallocate in the hash table.
        U32 preallocate_size{1000};
    };

    SpatialHashGrid();
    explicit SpatialHashGrid(Options options) noexcept;
    SpatialHashGrid(Alloc *alloc, Options options) noexcept;

    SpatialHashGrid(const SpatialHashGrid &) noexcept = default;
    SpatialHashGrid(SpatialHashGrid &&) noexcept = default;
    SpatialHashGrid &operator=(const SpatialHashGrid &) noexcept = default;
    SpatialHashGrid &operator=(SpatialHashGrid &&) noexcept = default;
    ~SpatialHashGrid() noexcept = default;

    /// @brief Pushes a thing into the grid at the given cell coordinates.
    void push(Thing thing, S32 x, S32 y, S32 z) noexcept;

    /// @brief Clears the grid, removing all entries.
    void clear() noexcept;

    /// @brief Sorts the entries by their hash value.
    void sort() noexcept;

    /// @brief Returns a const reference to the internal entries array.
    const DynamicArray<Entry> &entries() const noexcept;

    /// @brief Returns the options this grid was configured with.
    const Options &options() const noexcept;

private:
    DynamicArray<Entry> m_entries;
    Options m_options;
};

} // namespace fr::impl
