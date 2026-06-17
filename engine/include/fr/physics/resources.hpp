/**
 * @file resources.hpp
 * @author Kiju
 *
 * @brief Contains physics resource definitions:
 * - settings for the simulation
 * - spatial hash grid for broad-phase collision detection
 * - manifold pool for narrow-phase collision results
 */

#pragma once

#include "fr/core/math.hpp"
#include "fr/core/shape.hpp"
#include "fr/physics/collision_manifold_pool.hpp"
#include "fr/physics/spatial_hash_grid.hpp"

namespace fr {
struct PhysicsOptions {
    /// @brief Gravitational acceleration in world space.
    Vec3 gravity{0.0f, 0.0f, 0.0f};

    /// @brief Number of constraint solver iterations per step.
    USize iteration_count{1};

    FR_SHAPE({
        FR_PROP(gravity);
        FR_PROP(iteration_count);
    })
};

struct PhysicsState {
    PhysicsOptions options;
    impl::SpatialHashGrid grid;
    impl::CollisionManifoldPool manifold_pool;
    F32 dt{1.0f / 60.0f};
    bool is_running{true};

    FR_SHAPE({
        FR_PROP(options);
        FR_PROP(is_running);
    })
};
} // namespace fr
