/**
 * @file spatial_hash_grid.cpp
 * @author Kiju
 *
 * @brief Spatial hash grid implementation.
 */

#include "fr/physics/spatial_hash_grid.hpp"

#include "fr/core/algo.hpp"
#include "fr/core/ctx.hpp"

namespace fr::impl {

namespace {

constexpr SpatialHashGrid::Hash hash_cell(S32 x, S32 y, S32 z, U32 table_size) noexcept {
    constexpr U32 p1 = 73856093;
    constexpr U32 p2 = 19349663;
    constexpr U32 p3 = 83492791;

    // Cast to unsigned to avoid undefined behavior on negative coordinates.
    const SpatialHashGrid::Hash h = (static_cast<SpatialHashGrid::Hash>(x) * p1) ^
                                    (static_cast<SpatialHashGrid::Hash>(y) * p2) ^
                                    (static_cast<SpatialHashGrid::Hash>(z) * p3);

    return h % table_size;
}

} // namespace

SpatialHashGrid::SpatialHashGrid()
    : SpatialHashGrid(get_ambient_ctx().alloc, {}) {
}

SpatialHashGrid::SpatialHashGrid(Options options) noexcept
    : SpatialHashGrid(get_ambient_ctx().alloc, options) {
}

SpatialHashGrid::SpatialHashGrid(Alloc *alloc, Options options) noexcept
    : m_entries(alloc),
      m_options(options) {
    m_entries.reserve(m_options.preallocate_size);
}

void SpatialHashGrid::push(Thing thing, S32 x, S32 y, S32 z) noexcept {
    m_entries.push_back(Entry{.thing = thing, .hash = hash_cell(x, y, z, m_options.table_size)});
}

void SpatialHashGrid::clear() noexcept {
    m_entries.clear();
}

void SpatialHashGrid::sort() noexcept {
    radix_sort_key(m_entries.slice_mut(), [](const Entry &e) { return e.hash; });
}

const DynamicArray<SpatialHashGrid::Entry> &SpatialHashGrid::entries() const noexcept {
    return m_entries;
}

const SpatialHashGrid::Options &SpatialHashGrid::options() const noexcept {
    return m_options;
}

} // namespace fr::impl
