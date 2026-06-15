/**
 * @file collision_manifold_pool.cpp
 * @author Kiju
 *
 * @brief Arena-backed pool for collision manifolds.
 */

#include "fr/physics/collision_manifold_pool.hpp"

#include <cstring>

#include "fr/core/ctx.hpp"

namespace fr::impl {

CollisionManifoldPool::CollisionManifoldPool()
    : CollisionManifoldPool(get_ambient_ctx().alloc, DEFAULT_CAPACITY) {
}

CollisionManifoldPool::CollisionManifoldPool(Alloc *alloc, USize capacity) {
    m_alloc = alloc;
    m_capacity = capacity;

    const USize bytes = capacity * sizeof(CollisionManifold);
    m_raw_buffer = static_cast<Byte *>(alloc->allocate(bytes, alignof(CollisionManifold)));
    m_arena = ArenaAlloc(m_raw_buffer, bytes, "CollisionManifoldPool");
}

CollisionManifoldPool::~CollisionManifoldPool() {
    if (m_raw_buffer && m_alloc) {
        m_alloc->deallocate(m_raw_buffer, m_capacity * sizeof(CollisionManifold),
                            alignof(CollisionManifold));
    }
}

CollisionManifoldPool::CollisionManifoldPool(const CollisionManifoldPool &other) noexcept {
    m_alloc = other.m_alloc;
    m_capacity = other.m_capacity;
    m_count = other.m_count;

    const USize bytes = m_capacity * sizeof(CollisionManifold);
    m_raw_buffer = static_cast<Byte *>(m_alloc->allocate(bytes, alignof(CollisionManifold)));
    m_arena = ArenaAlloc(m_raw_buffer, bytes, "CollisionManifoldPool");

    std::memcpy(m_raw_buffer, other.m_raw_buffer, bytes);
}

CollisionManifoldPool &
CollisionManifoldPool::operator=(const CollisionManifoldPool &other) noexcept {
    if (this != &other) {
        m_alloc = other.m_alloc;
        m_capacity = other.m_capacity;
        m_count = other.m_count;

        const USize bytes = m_capacity * sizeof(CollisionManifold);
        m_raw_buffer = static_cast<Byte *>(m_alloc->allocate(bytes, alignof(CollisionManifold)));
        m_arena = ArenaAlloc(m_raw_buffer, bytes, "CollisionManifoldPool");

        std::memcpy(m_raw_buffer, other.m_raw_buffer, bytes);
    }

    return *this;
}

CollisionManifoldPool::CollisionManifoldPool(CollisionManifoldPool &&other) noexcept {
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
}

CollisionManifoldPool &CollisionManifoldPool::operator=(CollisionManifoldPool &&other) noexcept {
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
}

void CollisionManifoldPool::clear() noexcept {
    m_arena.reset();
    m_count = 0;
}

bool CollisionManifoldPool::push(CollisionManifold m) noexcept {
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

Slice<const CollisionManifold> CollisionManifoldPool::manifolds() const noexcept {
    return {reinterpret_cast<const CollisionManifold *>(m_raw_buffer), m_count};
}

USize CollisionManifoldPool::count() const noexcept {
    return m_count;
}

USize CollisionManifoldPool::capacity() const noexcept {
    return m_capacity;
}

} // namespace fr::impl
