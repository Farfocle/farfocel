/**
 * @file collision_manifold_pool.hpp
 * @author Kiju
 *
 * @brief Arena-backed pool for collision manifolds.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/physics/parts.hpp"

namespace fr::impl {

/// @brief Fixed-capacity pool of `CollisionManifold` objects backed by an `ArenaAlloc`.
class CollisionManifoldPool {
public:
    /// @brief Default capacity when none is supplied.
    static constexpr USize DEFAULT_CAPACITY = 1024;

    // ------------------------------------------------------------ Constructors

    CollisionManifoldPool()
        : CollisionManifoldPool(get_ambient_ctx().alloc, DEFAULT_CAPACITY) {
    }

    CollisionManifoldPool(Alloc *alloc, USize capacity) {
        m_alloc = alloc;
        m_capacity = capacity;
        const USize bytes = capacity * sizeof(CollisionManifold);
        m_raw_buffer = static_cast<Byte *>(alloc->allocate(bytes, alignof(CollisionManifold)));
        m_arena = ArenaAlloc(m_raw_buffer, bytes, "CollisionManifoldPool");
    }

    ~CollisionManifoldPool() {
        if (m_raw_buffer && m_alloc) {
            m_alloc->deallocate(m_raw_buffer, m_capacity * sizeof(CollisionManifold),
                                alignof(CollisionManifold));
        }
    }

    CollisionManifoldPool(const CollisionManifoldPool &other) noexcept {
        m_alloc = other.m_alloc;
        m_capacity = other.m_capacity;
        m_count = other.m_count;

        const USize bytes = m_capacity * sizeof(CollisionManifold);
        m_raw_buffer = static_cast<Byte *>(m_alloc->allocate(bytes, alignof(CollisionManifold)));
        m_arena = ArenaAlloc(m_raw_buffer, bytes, "CollisionManifoldPool");

        std::memcpy(m_raw_buffer, other.m_raw_buffer, bytes);
    };

    CollisionManifoldPool &operator=(const CollisionManifoldPool &other) noexcept {
        if (this != &other) {
            m_alloc = other.m_alloc;
            m_capacity = other.m_capacity;
            m_count = other.m_count;

            const USize bytes = m_capacity * sizeof(CollisionManifold);
            m_raw_buffer =
                static_cast<Byte *>(m_alloc->allocate(bytes, alignof(CollisionManifold)));
            m_arena = ArenaAlloc(m_raw_buffer, bytes, "CollisionManifoldPool");

            std::memcpy(m_raw_buffer, other.m_raw_buffer, bytes);
        }

        return *this;
    };

    CollisionManifoldPool(CollisionManifoldPool &&other) noexcept {
        m_alloc = other.m_alloc;
        m_capacity = other.m_capacity;
        m_count = other.m_count;
        m_raw_buffer = other.m_raw_buffer;
        m_arena = other.m_arena;

        other.m_alloc = nullptr;
        other.m_capacity = 0;
        other.m_count = 0;
        other.m_raw_buffer = nullptr;
        other.m_arena = ArenaAlloc();
    };

    CollisionManifoldPool &operator=(CollisionManifoldPool &&other) noexcept {
        m_alloc = other.m_alloc;
        m_capacity = other.m_capacity;
        m_count = other.m_count;
        m_raw_buffer = other.m_raw_buffer;
        m_arena = other.m_arena;

        other.m_alloc = nullptr;
        other.m_capacity = 0;
        other.m_count = 0;
        other.m_raw_buffer = nullptr;
        other.m_arena = ArenaAlloc();

        return *this;
    };

    // -------------------------------------------------------------------- API

    /// @brief Resets the pool, discarding all manifolds in O(1).
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
    [[nodiscard]] USize count() const noexcept {
        return m_count;
    }

    /// @brief Maximum number of manifolds the pool can hold.
    [[nodiscard]] USize capacity() const noexcept {
        return m_capacity;
    }

private:
    Byte *m_raw_buffer{nullptr};
    Alloc *m_alloc{nullptr};
    ArenaAlloc m_arena{};
    USize m_capacity{0};
    USize m_count{0};
};

} // namespace fr::impl
