/**
 * @file collision_manifold_pool.hpp
 * @author Kiju
 *
 * @brief Arena-backed pool for collision manifolds.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/physics/parts.hpp"

namespace fr::impl {

/// @brief Fixed-capacity pool of `CollisionManifold` objects backed by an `ArenaAlloc`.
class CollisionManifoldPool {
public:
    static constexpr USize DEFAULT_CAPACITY = 1024;

    CollisionManifoldPool();
    CollisionManifoldPool(Alloc *alloc, USize capacity);
    ~CollisionManifoldPool();

    CollisionManifoldPool(const CollisionManifoldPool &other) noexcept;
    CollisionManifoldPool &operator=(const CollisionManifoldPool &other) noexcept;
    CollisionManifoldPool(CollisionManifoldPool &&other) noexcept;
    CollisionManifoldPool &operator=(CollisionManifoldPool &&other) noexcept;

    /// @brief Resets the pool, discarding all manifolds in O(1).
    void clear() noexcept;

    /**
     * @brief Pushes a manifold into the pool.
     * @return `true` on success; `false` if the pool is full.
     */
    bool push(CollisionManifold m) noexcept;

    /// @brief Returns a read-only view over all stored manifolds.
    [[nodiscard]] Slice<const CollisionManifold> manifolds() const noexcept;

    /// @brief Number of manifolds currently in the pool.
    [[nodiscard]] USize count() const noexcept;

    /// @brief Maximum number of manifolds the pool can hold.
    [[nodiscard]] USize capacity() const noexcept;

private:
    Byte *m_raw_buffer{nullptr};
    Alloc *m_alloc{nullptr};
    ArenaAlloc m_arena{};
    USize m_capacity{0};
    USize m_count{0};
};

} // namespace fr::impl
