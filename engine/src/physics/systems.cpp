/**
 * @file systems.cpp
 * @author Kiju
 *
 * @brief Physics system implementations.
 *
 * @details
 * Systems are registered with the World and run each frame in stage order.
 * The collision pipeline is two-pass:
 *
 *  1. **Broadphase** (`broadphase_system`): transforms each collider into world space,
 *     inserts it into a `SpatialHashGrid` (one or more cells per collider), then sorts the
 *     entries by cell hash.  Things that share a cell hash are candidate collision pairs.
 *
 *  2. **Narrowphase** (`narrowphase_system`): iterates runs of same-hash entries from the
 *     sorted grid.  For every candidate pair it transforms both colliders to world space,
 *     performs an exact `check_collision` test, computes the `CollisionManifold` via
 *     `compute_manifold`, stores it in the `CollisionManifoldPool`, and records the contact
 *     in each thing's `CollisionEventsPart`.
 *
 * Duplicate manifolds across multiple shared cells are suppressed by scanning the pool before
 * inserting - acceptable overhead for the expected contact counts.
 */

#include <cmath>

#include "fr/core/math.hpp"
#include "fr/data/world.hpp"
#include "fr/physics/parts.hpp"
#include "fr/physics/resources.hpp"
#include "fr/physics/systems.hpp"
#include "glm/geometric.hpp"

namespace fr {

// ===================================================================== Helpers

/**
 * @brief Transforms a local-space AABB and its collider offset into world space.
 *
 * @details
 * Centre is mapped by the full 4x4 TRS matrix (handles translation + rotation + scale).
 * Half-extents are expanded using the absolute column sums of the upper 3x3, which gives
 * a tight world-space AABB without needing to test all 8 corners.
 *
 * @param local Local-space AABB (no offset applied yet).
 * @param offset Local-space collider offset (added to the AABB centre before transforming).
 * @param mat World transform matrix (column-major, glm layout).
 */
static AABB do_aabb_to_world(const AABB &local, const Vec3 &offset, const Mat4 &mat) noexcept {
    const Vec3 local_center = local.center() + offset;
    const Vec3 world_center = Vec3(mat * Vec4(local_center, 1.0f));

    const Vec3 local_half = local.half_extents();
    Vec3 world_half{0.0f};

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            world_half[i] += std::abs(mat[j][i]) * local_half[j];
        }
    }

    return AABB::from_center(world_center, world_half);
}

/**
 * @brief Transforms a local-space sphere and its collider offset into world space.
 *
 * @details
 * Centre is mapped by the full 4x4 matrix.
 * Radius is scaled by the largest column magnitude of the upper 3x3, which handles
 * non-uniform scale correctly (the sphere becomes the smallest enclosing sphere).
 *
 * @param local Local-space sphere.
 * @param offset Local-space collider offset.
 * @param mat World transform matrix.
 */
static Sphere do_sphere_to_world(const Sphere &local, const Vec3 &offset,
                                 const Mat4 &mat) noexcept {
    const Vec3 world_center = Vec3(mat * Vec4(local.center + offset, 1.0f));

    // Scale radius by the maximum column magnitude of the upper 3x3.
    F32 max_scale = 0.0f;
    for (int j = 0; j < 3; ++j) {
        const Vec3 col{mat[j]};
        max_scale = math::max(max_scale, glm::length(col));
    }

    return Sphere{world_center, local.radius * max_scale};
}

/**
 * @brief Transforms a local-space `ColliderPart` into world space.
 *
 * @param local Source collider (local space, offset included).
 * @param mat World transform matrix.
 * @return A new `ColliderPart` with world-space primitive; offset is zeroed (already applied).
 */
static ColliderPart do_collider_to_world(const ColliderPart &local, const Mat4 &mat) noexcept {
    if (local.kind == ColliderKind::AABB) {
        return ColliderPart::make_aabb(do_aabb_to_world(local.aabb, local.offset, mat));
    }

    return ColliderPart::make_sphere(do_sphere_to_world(local.sphere, local.offset, mat));
}

/**
 * @brief Pushes all cells covered by a world-space AABB into the spatial hash grid.
 *
 * @param grid Grid to push into.
 * @param thing Owner of the collider.
 * @param ws_aabb World-space bounding box.
 */
static void do_push_aabb_cells(impl::SpatialHashGrid &grid, Thing thing,
                               const AABB &ws_aabb) noexcept {
    const F32 cell_size = grid.options().cell_size;
    const auto cell = [&](F32 v) { return static_cast<S32>(std::floor(v / cell_size)); };

    const S32 x0 = cell(ws_aabb.min.x), x1 = cell(ws_aabb.max.x);
    const S32 y0 = cell(ws_aabb.min.y), y1 = cell(ws_aabb.max.y);
    const S32 z0 = cell(ws_aabb.min.z), z1 = cell(ws_aabb.max.z);

    for (S32 x = x0; x <= x1; ++x) {
        for (S32 y = y0; y <= y1; ++y) {
            for (S32 z = z0; z <= z1; ++z) {
                grid.push(thing, x, y, z);
            }
        }
    }
}

/**
 * @brief Pushes all cells covered by a world-space sphere into the spatial hash grid.
 *
 * @param grid Grid to push into.
 * @param thing Owner of the collider.
 * @param ws_sphere World-space bounding sphere.
 */
static void do_push_sphere_cells(impl::SpatialHashGrid &grid, Thing thing,
                                 const Sphere &ws_sphere) noexcept {
    const F32 cell_size = grid.options().cell_size;
    const auto cell = [&](F32 v) { return static_cast<S32>(std::floor(v / cell_size)); };
    const S32 r = static_cast<S32>(std::ceil(ws_sphere.radius / cell_size));

    const S32 cx = cell(ws_sphere.center.x);
    const S32 cy = cell(ws_sphere.center.y);
    const S32 cz = cell(ws_sphere.center.z);

    for (S32 x = cx - r; x <= cx + r; ++x) {
        for (S32 y = cy - r; y <= cy + r; ++y) {
            for (S32 z = cz - r; z <= cz + r; ++z) {
                grid.push(thing, x, y, z);
            }
        }
    }
}

// ===================================================================== Systems

/**
 * @brief Broadphase collision detection system.
 *
 * @details
 * Clears the `SpatialHashGrid` resource, then queries all things that have both a
 * `WorldTransformPart` and a `ColliderPart`. Each collider is transformed to world space
 * and inserted into every grid cell it touches. Finally the grid is sorted by cell hash so
 * the narrowphase can iterate same-hash runs efficiently.
 */
void broadphase_system(Scope scope) {
    PhysicsState &state = scope.get_resource<PhysicsState>();
    state.grid.clear();

    for (auto [thing, wt, collider] : scope.query<WorldTransformPart, ColliderPart>()) {
        if (collider.kind == ColliderKind::AABB) {
            const AABB ws = do_aabb_to_world(collider.aabb, collider.offset, wt.mat);
            do_push_aabb_cells(state.grid, thing, ws);
        } else {
            const Sphere ws = do_sphere_to_world(collider.sphere, collider.offset, wt.mat);
            do_push_sphere_cells(state.grid, thing, ws);
        }
    }

    state.grid.sort();
}

/**
 * @brief Narrowphase collision detection system.
 *
 * @details
 * For each run of same-hash entries in the sorted grid, all unique pairs of distinct things
 * are tested:
 *  - World-space colliders are reconstructed from `WorldTransformPart` + `ColliderPart`.
 *  - `check_collision` performs the exact overlap test.
 *  - `compute_manifold` computes the contact normal, point, and penetration depth.
 *  - The manifold is pushed to `PhysicsState::manifold_pool`.
 *  - Both things' `CollisionEventsPart` (if present) are updated.
 *
 * Duplicate manifolds (same pair detected in multiple shared cells) are suppressed by
 * scanning the pool before inserting.
 *
 * `CollisionEventsPart` contact lists are cleared at the start of the system, so each
 * thing's contacts accurately reflect only the current frame's contacts.
 */
void narrowphase_system(Scope scope) {
    PhysicsState &state = scope.get_resource<PhysicsState>();

    // Reset per-frame state.
    state.manifold_pool.clear();
    for (auto [thing, events] : scope.query<CollisionEventsPart>()) {
        (void)thing;
        events.count = 0;
    }

    const auto &entries = state.grid.entries();
    const USize n = entries.size();

    USize i = 0;
    while (i < n) {
        // Find the end of this hash run [i, j).
        USize j = i + 1;
        while (j < n && entries[j].hash == entries[i].hash) {
            ++j;
        }

        // Check all unique pairs within the run.
        for (USize a = i; a < j; ++a) {
            for (USize b = a + 1; b < j; ++b) {
                const Thing ta = entries[a].thing;
                const Thing tb = entries[b].thing;

                if (ta == tb) {
                    continue;
                }

                // Fetch world-transform and collider for both things.
                WorldTransformPart *wt_a = scope.try_get<WorldTransformPart>(ta);
                ColliderPart *col_a = scope.try_get<ColliderPart>(ta);
                WorldTransformPart *wt_b = scope.try_get<WorldTransformPart>(tb);
                ColliderPart *col_b = scope.try_get<ColliderPart>(tb);

                if (!wt_a || !col_a || !wt_b || !col_b) {
                    continue;
                }

                const ColliderPart ws_a = do_collider_to_world(*col_a, wt_a->mat);
                const ColliderPart ws_b = do_collider_to_world(*col_b, wt_b->mat);

                if (!check_collision(ws_a, ws_b)) {
                    continue;
                }

                // Suppress duplicate pairs that arise when two things share multiple cells.
                bool already_recorded = false;
                for (const CollisionManifold &m : state.manifold_pool.manifolds()) {
                    if ((m.a == ta && m.b == tb) || (m.a == tb && m.b == ta)) {
                        already_recorded = true;
                        break;
                    }
                }
                if (already_recorded) {
                    continue;
                }

                ManifoldResult mr = compute_manifold(ws_a, ws_b);
                if (!mr.hit) {
                    continue;
                }

                mr.manifold.a = ta;
                mr.manifold.b = tb;
                state.manifold_pool.push(mr.manifold);

                // Record the contact in both things' event parts (if they have one).
                if (CollisionEventsPart *ea = scope.try_get<CollisionEventsPart>(ta)) {
                    ea->insert(tb);
                }
                if (CollisionEventsPart *eb = scope.try_get<CollisionEventsPart>(tb)) {
                    eb->insert(ta);
                }
            }
        }

        i = j;
    }
}

// ======================================================================= Stubs

/// @brief Stub — applies accumulated forces and gravity to rigid bodies.
void apply_forces_system(Scope /*scope*/) {
}

/// @brief Stub — solves velocity/position constraints from the manifold pool.
void rigit_body_contraist_system(Scope /*scope*/) {
}

/// @brief Stub — integrates rigid body velocity and position (symplectic Euler).
void rigit_body_integration_system(Scope /*scope*/) {
}

} // namespace fr
