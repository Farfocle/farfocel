/**
 * @file collision_manifold_pool.hpp
 * @author Kiju
 *
 * @brief Arena-backed pool for collision manifolds.
 *
 * @details
 * `CollisionManifoldPool` pre-allocates a flat buffer from a parent `Alloc` at construction
 * time and wraps it in an `ArenaAlloc`. Each frame the narrowphase pushes manifolds into the
 * pool; at the start of the next frame `clear()` resets the arena in O(1) — no individual
 * frees, no heap churn.
 *
 * Typical usage:
 * @code
 *   // constructed once as part of PhysicsState resource
 *   impl::CollisionManifoldPool pool;          // uses ambient alloc, capacity = 1024
 *
 *   // narrowphase
 *   pool.clear();
 *   pool.push({ta, tb, normal, point, depth});
 *
 *   // constraint solver / event dispatch
 *   for (const CollisionManifold& m : pool.manifolds()) { ... }
 * @endcode
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/physics/parts.hpp"

namespace fr::impl {

/**
 * @brief Fixed-capacity pool of `CollisionManifold` objects backed by an `ArenaAlloc`.
 *
 * @note Non-copyable, non-movable — intended for use as a long-lived resource.
 */
class CollisionManifoldPool {
public:
    /// @brief Default capacity when none is supplied.
    static constexpr USize DEFAULT_CAPACITY = 1024;

    // ------------------------------------------------------------ Constructors

    /**
     * @brief Constructs the pool with `DEFAULT_CAPACITY` manifolds,
     *        allocating from the ambient context's allocator.
     */
    CollisionManifoldPool()
        : CollisionManifoldPool(get_ambient_ctx().alloc, DEFAULT_CAPACITY) {}

    /**
     * @brief Constructs the pool.
     *
     * @param alloc    Parent allocator that owns the backing buffer for the pool's lifetime.
     * @param capacity Maximum number of manifolds that can be held simultaneously.
     */
    CollisionManifoldPool(Alloc *alloc, USize capacity) {
        m_alloc = alloc;
        m_capacity = capacity;
        const USize bytes = capacity * sizeof(CollisionManifold);
        m_raw_buffer =
            static_cast<Byte *>(alloc->allocate(bytes, alignof(CollisionManifold)));
        m_arena = ArenaAlloc(m_raw_buffer, bytes, "CollisionManifoldPool");
    }

    ~CollisionManifoldPool() {
        if (m_raw_buffer && m_alloc) {
            m_alloc->deallocate(m_raw_buffer, m_capacity * sizeof(CollisionManifold),
                                alignof(CollisionManifold));
        }
    }

    // Pool is a heavyweight resource — copying/moving would alias or lose the backing buffer.
    CollisionManifoldPool(const CollisionManifoldPool &) = delete;
    CollisionManifoldPool &operator=(const CollisionManifoldPool &) = delete;
    CollisionManifoldPool(CollisionManifoldPool &&) = delete;
    CollisionManifoldPool &operator=(CollisionManifoldPool &&) = delete;

    // -------------------------------------------------------------------- API

    /**
     * @brief Resets the pool, discarding all manifolds in O(1).
     *
     * @details Resets the arena bump pointer — no individual destructor calls needed since
     * `CollisionManifold` is a plain aggregate.
     */
    void clear() noexcept {
        m_arena.reset();
        m_count = 0;
    }

    /**
     * @brief Pushes a manifold into the pool.
     *
     * @param m Manifold to store.
     * @return `true` on success; `false` if the pool is full (capacity exhausted).
     */
    bool push(CollisionManifold m) noexcept {
        if (m_count >= m_capacity) {
            return false;
        }

        void *slot = m_arena.try_allocate(sizeof(CollisionManifold), alignof(CollisionManifold));
        if (!slot) {
            return false;
        }

        *static_cast<CollisionManifold *>(slot) = m;
        ++m_count;

        return true;
    }

    /// @brief Returns a read-only view over all stored manifolds.
    [[nodiscard]] Slice<const CollisionManifold> manifolds() const noexcept {
        return {reinterpret_cast<const CollisionManifold *>(m_raw_buffer), m_count};
    }

    /// @brief Number of manifolds currently in the pool.
    [[nodiscard]] USize count() const noexcept { return m_count; }

    /// @brief Maximum number of manifolds the pool can hold.
    [[nodiscard]] USize capacity() const noexcept { return m_capacity; }

private:
    Byte *m_raw_buffer{nullptr};
    Alloc *m_alloc{nullptr};
    ArenaAlloc m_arena{};
    USize m_capacity{0};
    USize m_count{0};
};

} // namespace fr::impl
