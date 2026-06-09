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
    Vec3 gravity{};
    USize iteration_count{};

    FR_SHAPE({
        FR_PROP(gravity);
        FR_PROP(iteration_count);
    })
};

struct PhysicsState {
    PhysicsOptions options;
    impl::SpatialHashGrid grid;
    impl::CollisionManifoldPool manifold_pool;

    FR_SHAPE({ FR_PROP(options); })
};
} // namespace fr
