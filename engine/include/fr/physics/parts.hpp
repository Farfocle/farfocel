/**
 * @file parts.hpp
 * @author Kiju
 *
 * @brief Collection of physics parts for the physics engine.
 *
 * @details All physics state lives as Parts on Things. The physics system reads and writes
 * these parts each frame. Storing *inverse* mass and inertia avoids division in the integrator
 * hot path - a thing with inv_mass == 0 is treated as an infinite-mass static body.
 *
 * @note Coordinate convention: right-handed, Y-up, consistent with GLM defaults.
 */

#pragma once

#include "fr/core/array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"
#include <cmath>

#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"

namespace fr {

// =================================================================== Transform

/**
 * @brief The transform of a thing relative to its parent (or world origin if it has none).
 *
 * @details Decomposed form: position, orientation quaternion, and per-axis scale.
 * The physics and rendering systems reconstruct a matrix from these as needed via `to_mat4()`.
 */
struct LocalTransformPart {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    FR_SHAPE({
        FR_PROP(pos);
        FR_PROP(rot);
        FR_PROP(scale);
    })

    /// @brief Returns a transform at the origin with identity rotation and unit scale.
    static LocalTransformPart identity() noexcept {
        return {};
    }

    /// @brief Returns a transform at `p` with identity rotation and unit scale.
    static LocalTransformPart from_pos(Vec3 p) noexcept {
        LocalTransformPart t;
        t.pos = p;
        return t;
    }

    /**
     * @brief Builds the TRS matrix: T * R * S.
     * @note Column 3 carries the translation; the upper-left 3x3 is R*S.
     */
    Mat4 to_mat4() const noexcept {
        Mat4 m = glm::mat4_cast(rot);
        m = glm::scale(m, scale);
        m[3] = Vec4(pos, 1.0f);

        return m;
    }
};

/**
 * @brief The fully composed world-space transform of a thing.
 *
 * @details Computed by the transform propagation system from the @ref LocalTransform hierarchy.
 * `mat` is the cached TRS matrix ready to upload to the GPU.
 * `pos` and `rot` are kept separate for physics queries that need world position/orientation
 * without decomposing the matrix.
 */
struct WorldTransformPart {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    Mat4 mat{1.0f};

    FR_SHAPE({
        FR_PROP(pos);
        FR_PROP(rot);
        FR_PROP(mat);
    })

    /// @brief Returns an identity world transform.
    static WorldTransformPart identity() noexcept {
        return {};
    }
};

// =========================================================================== Mass

/**
 * @brief Rigid body mass properties.
 *
 * @details
 * Stores *inverse* quantities to avoid per-frame division in the physics integrator.
 * Set `inv_mass = 0` (and `inv_inertia = Mat3(0)`) for a static / kinematic body.
 * The inertia tensor is stored in **local body space**; the physics system rotates it to world
 * space each step as needed: I_world = R * inv_inertia * R^T.
 */
struct MassPart {
    Mat3 inv_inertia{1.0f};
    F32 inv_mass{1.0f};
    F32 restitution{0.5f};

    FR_SHAPE({
        FR_PROP(inv_inertia);
        FR_PROP(inv_mass);
        FR_PROP(restitution);
    })

    /// @brief Returns the actual mass value (0 if this is a static / infinite-mass body).
    F32 mass() const noexcept {
        return inv_mass > 0.0f ? 1.0f / inv_mass : 0.0f;
    }

    /// @brief Creates a mass with a scalar `m` and identity (isotropic) inertia tensor.
    static MassPart from_mass(F32 m, F32 restitution = 0.5f) noexcept {
        FR_ASSERT(m > 0.0f, "mass must be positive");
        return {Mat3(1.0f / m), 1.0f / m, restitution};
    }

    /// @brief Creates an immovable body (inv_mass = 0, zero inertia).
    static MassPart infinite() noexcept {
        return {Mat3(0.0f), 0.0f, 0.0f};
    }

    /// @brief Solid sphere: I = (2/5) * m * r^2 on each diagonal axis.
    static MassPart from_sphere(F32 m, F32 radius, F32 restitution = 0.5f) noexcept {
        FR_ASSERT(m > 0.0f && radius > 0.0f, "mass and radius must be positive");
        const F32 i_inv = 5.0f / (2.0f * m * radius * radius);
        return {Mat3(i_inv), 1.0f / m, restitution};
    }

    /**
     * @brief Solid box: Ix = (1/3)*m*(hy^2+hz^2), Iy = (1/3)*m*(hx^2+hz^2), Iz =
     * (1/3)*m*(hx^2+hy^2).
     *
     * @param half_extents Half-dimensions along each axis (hx, hy, hz).
     */
    static MassPart from_box(F32 m, Vec3 half_extents, F32 restitution = 0.5f) noexcept {
        FR_ASSERT(m > 0.0f, "mass must be positive");

        const F32 k = m / 3.0f;
        const F32 hx2 = half_extents.x * half_extents.x;
        const F32 hy2 = half_extents.y * half_extents.y;
        const F32 hz2 = half_extents.z * half_extents.z;

        Mat3 inv_i(0.0f);
        inv_i[0][0] = 1.0f / (k * (hy2 + hz2));
        inv_i[1][1] = 1.0f / (k * (hx2 + hz2));
        inv_i[2][2] = 1.0f / (k * (hx2 + hy2));

        return {inv_i, 1.0f / m, restitution};
    }
};

// ========================================================= Broad-Phase Volumes

/**
 * @brief Axis-Aligned Bounding Box in world space.
 * @note Defined by its minimum and maximum corner points. Used for broad-phase collision detection.
 */
struct AABB {
    Vec3 min{};
    Vec3 max{};

    FR_SHAPE({
        FR_PROP(min);
        FR_PROP(max);
    })

    /// @brief Returns the center of the box.
    Vec3 center() const noexcept {
        return (min + max) * 0.5f;
    }

    /// @brief Returns the half-extents (half the size along each axis).
    Vec3 half_extents() const noexcept {
        return (max - min) * 0.5f;
    }

    /// @brief Returns true if `point` is inside or on the boundary.
    bool contains(const Vec3 &point) const noexcept {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    /// @brief Returns true if this box overlaps `other` (touching counts as overlap).
    bool overlaps(const AABB &other) const noexcept {
        return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y &&
               max.y >= other.min.y && min.z <= other.max.z && max.z >= other.min.z;
    }

    /// @brief Creates an AABB from a center point and half-extents.
    static AABB from_center(Vec3 center, Vec3 half_extents) noexcept {
        return {center - half_extents, center + half_extents};
    }
};

/**
 * @brief Bounding sphere in world space.
 * @note Defined by its center point and radius.
 */
struct Sphere {
    Vec3 center{};
    F32 radius{1.0f};

    FR_SHAPE({
        FR_PROP(center);
        FR_PROP(radius);
    })

    /// @brief Returns true if `point` is inside or on the surface of the sphere.
    bool contains(const Vec3 &point) const noexcept {
        const Vec3 offset = point - center;
        return glm::dot(offset, offset) <= radius * radius;
    }

    /// @brief Returns true if this sphere overlaps `other`.
    bool overlaps(const Sphere &other) const noexcept {
        const F32 combined = radius + other.radius;
        return glm::distance(center, other.center) <= combined;
    }
};

// ==================================================================== Collider

/// @brief Identifies which collision primitive is active inside a `Collider`.
enum class ColliderKind : U8 { AABB, Sphere };

/**
 * @brief A broad-phase collision primitive attached to a thing.
 *
 * @details
 * Stores one of AABB or Sphere as a tagged union. `offset` is in local space and is
 * added when computing the world-space bounds so the primitive can be positioned
 * independently of the thing's origin (useful for off-center colliders).
 */
struct ColliderPart {
    ColliderKind kind{ColliderKind::AABB};
    union {
        AABB aabb;
        Sphere sphere;
    };

    Vec3 offset{};

    ColliderPart() noexcept {

    };

    /// @brief Creates an AABB collider with an optional local-space offset.
    static ColliderPart make_aabb(AABB box, Vec3 offset = {}) noexcept {
        ColliderPart c;
        c.kind = ColliderKind::AABB;
        c.aabb = box;
        c.offset = offset;

        return c;
    }

    /// @brief Creates a sphere collider with an optional local-space offset.
    static ColliderPart make_sphere(Sphere s, Vec3 offset = {}) noexcept {
        ColliderPart c;
        c.kind = ColliderKind::Sphere;
        c.sphere = s;
        c.offset = offset;

        return c;
    }
};

/// @brief Represents a collision manifold between two things.
struct CollisionManifold {
    Thing a;
    Thing b;

    Vec3 normal{};
    Vec3 point{};
    F32 depth{0};

    FR_SHAPE({
        FR_PROP(a);
        FR_PROP(b);
        FR_PROP(normal);
        FR_PROP(point);
        FR_PROP(depth);
    })
};

// ============================================================ Collision Events

/**
 * @brief Records the things that collided with a given thing this frame.
 *
 * @details
 * `count` is the number of valid entries; slots beyond `count` are stale/nil.
 * Capped at MAX_COLLISIONS - excess contacts are silently dropped (broad phase
 * should keep this small in practice).
 */
struct CollisionEventsPart {
    static constexpr USize MAX_COLLISIONS = 16;

    USize count{0};
    Array<Thing, MAX_COLLISIONS> contacts{
        Array<Thing, MAX_COLLISIONS>::from_repeated(Thing::nil())};

    /// @brief Returns true if `p` thing appears in the current contact list.
    bool has(Thing thing) const noexcept {
        for (USize i = 0; i < count; ++i) {
            if (contacts[i] == thing) {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Inserts `thing` into the contact list if there is room.
     * @return True if the contact was inserted, false if the list is full.
     */
    bool insert(Thing thing) {
        if (count < MAX_COLLISIONS) {
            contacts[count++] = thing;
            return true;
        }

        return false;
    }
};

// ============================================================ Physics Material

/**
 * @brief Surface response parameters for collision resolution.
 *
 * @details
 * When two bodies collide, the solver typically combines their materials
 * (e.g. by averaging or taking the minimum) to compute a single friction and
 * restitution value for the contact.
 */
struct PhysicsMaterialPart {
    F32 friction{0.5f};
    F32 restitution{0.5f};

    FR_SHAPE({
        FR_PROP(friction);
        FR_PROP(restitution);
    })
};

// ============================================================= Collision Tests

/// @brief Returns true if two AABBs overlap (touching counts).
inline bool check_collision(const AABB &a, const AABB &b) noexcept {
    return a.overlaps(b);
}

/// @brief Returns true if two spheres overlap (touching counts).
inline bool check_collision(const Sphere &a, const Sphere &b) noexcept {
    return a.overlaps(b);
}

/**
 * @brief Returns true if an AABB and a sphere overlap.
 *
 * @details
 * Finds the closest point on the AABB to the sphere center, then checks
 * whether its distance to the center is within the sphere's radius.
 */
inline bool check_collision(const AABB &a, const Sphere &s) noexcept {
    const Vec3 closest = glm::clamp(s.center, a.min, a.max);
    const Vec3 delta = s.center - closest;
    return glm::dot(delta, delta) <= s.radius * s.radius;
}

/// @brief Sphere vs AABB — symmetric overload, delegates to AABB vs Sphere.
inline bool check_collision(const Sphere &s, const AABB &a) noexcept {
    return check_collision(a, s);
}

/**
 * @brief Dispatches a collision test between two Colliders based on their runtime kind.
 * @note Covers all four kind-pairs: AABB/AABB, Sphere/Sphere, AABB/Sphere, Sphere/AABB.
 */
inline bool check_collision(const ColliderPart &a, const ColliderPart &b) noexcept {
    if (a.kind == ColliderKind::AABB && b.kind == ColliderKind::AABB) {
        return check_collision(a.aabb, b.aabb);
    }

    if (a.kind == ColliderKind::Sphere && b.kind == ColliderKind::Sphere) {
        return check_collision(a.sphere, b.sphere);
    }

    if (a.kind == ColliderKind::AABB && b.kind == ColliderKind::Sphere) {
        return check_collision(a.aabb, b.sphere);
    }

    return check_collision(b.aabb, a.sphere);
}

// ======================================================== Manifold Computation

/**
 * @brief Result of a narrow-phase collision test.
 *
 * @details If `hit` is false, the `manifold` field is uninitialised and must not be read.
 * The `Thing` fields inside `manifold` are left as `Thing::nil()` — the caller (narrowphase
 * system) is responsible for filling them in before storing the manifold.
 */
struct ManifoldResult {
    bool hit{false};
    CollisionManifold manifold{};
};

/**
 * @brief Computes the contact manifold for two overlapping AABBs using SAT.
 *
 * @details Picks the axis with minimum penetration depth as the separation axis.
 * The manifold normal points from `b` toward `a` (resolves `a` when applied positively).
 * `point` is the centre of the overlap interval along the contact axis.
 *
 * @return `{false, {}}` when the AABBs do not overlap.
 */
inline ManifoldResult compute_manifold(const AABB &a, const AABB &b) noexcept {
    const F32 ox = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    const F32 oy = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    const F32 oz = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);

    if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) {
        return {false, {}};
    }

    // Pick the axis with the smallest penetration.
    Vec3 normal;
    F32 depth;
    if (ox <= oy && ox <= oz) {
        depth = ox;
        normal = {1.0f, 0.0f, 0.0f};
    } else if (oy <= ox && oy <= oz) {
        depth = oy;
        normal = {0.0f, 1.0f, 0.0f};
    } else {
        depth = oz;
        normal = {0.0f, 0.0f, 1.0f};
    }

    // Flip normal so it points from b toward a.
    const Vec3 d = a.center() - b.center();
    if (glm::dot(d, normal) < 0.0f) {
        normal = -normal;
    }

    // Contact point: centre of the overlap region along each axis.
    const Vec3 point = {
        (std::max(a.min.x, b.min.x) + std::min(a.max.x, b.max.x)) * 0.5f,
        (std::max(a.min.y, b.min.y) + std::min(a.max.y, b.max.y)) * 0.5f,
        (std::max(a.min.z, b.min.z) + std::min(a.max.z, b.max.z)) * 0.5f,
    };

    return {true, {Thing::nil(), Thing::nil(), normal, point, depth}};
}

/**
 * @brief Computes the contact manifold for two overlapping spheres.
 *
 * @details The manifold normal points from `b` toward `a`.
 * When the sphere centres coincide, an arbitrary up-vector is used to avoid a zero normal.
 *
 * @return `{false, {}}` when the spheres do not overlap.
 */
inline ManifoldResult compute_manifold(const Sphere &a, const Sphere &b) noexcept {
    const Vec3 delta = a.center - b.center; // from b toward a
    const F32 dist_sq = glm::dot(delta, delta);
    const F32 combined = a.radius + b.radius;

    if (dist_sq > combined * combined) {
        return {false, {}};
    }

    const F32 dist = std::sqrt(dist_sq);
    const Vec3 normal = (dist > 1e-6f) ? delta / dist : Vec3{0.0f, 1.0f, 0.0f};
    const F32 depth = combined - dist;

    // Contact point: surface of b along the collision normal.
    const Vec3 point = b.center + normal * b.radius;

    return {true, {Thing::nil(), Thing::nil(), normal, point, depth}};
}

/**
 * @brief Computes the contact manifold for an AABB and an overlapping sphere.
 *
 * @details Finds the closest point on the AABB to the sphere centre.
 * When the sphere centre is inside the AABB the nearest face is selected as the exit face.
 * The manifold normal points from `b` (sphere) toward `a` (AABB).
 *
 * @return `{false, {}}` when there is no overlap.
 */
inline ManifoldResult compute_manifold(const AABB &a, const Sphere &b) noexcept {
    const Vec3 closest = glm::clamp(b.center, a.min, a.max);
    const Vec3 delta = b.center - closest; // from closest-on-a toward b center
    const F32 dist_sq = glm::dot(delta, delta);

    if (dist_sq > b.radius * b.radius) {
        return {false, {}};
    }

    Vec3 normal;
    F32 depth;
    Vec3 point;

    if (dist_sq < 1e-10f) {
        // Sphere centre is inside the AABB: find the face it is closest to and push it out.
        const Vec3 d_min = b.center - a.min; // distances to each -face
        const Vec3 d_max = a.max - b.center; // distances to each +face

        // Find the axis and face with the smallest distance.
        F32 min_d = d_min.x;
        normal = {-1.0f, 0.0f, 0.0f}; // outward normal of the -x face

        if (d_min.y < min_d) {
            min_d = d_min.y;
            normal = {0.0f, -1.0f, 0.0f};
        }

        if (d_min.z < min_d) {
            min_d = d_min.z;
            normal = {0.0f, 0.0f, -1.0f};
        }

        if (d_max.x < min_d) {
            min_d = d_max.x;
            normal = {1.0f, 0.0f, 0.0f};
        }

        if (d_max.y < min_d) {
            min_d = d_max.y;
            normal = {0.0f, 1.0f, 0.0f};
        }

        if (d_max.z < min_d) {
            normal = {0.0f, 0.0f, 1.0f};
        }

        depth = b.radius + min_d;
        point = b.center;
    } else {
        const F32 dist = std::sqrt(dist_sq);

        // delta points from a's surface toward b; negate to get normal from b toward a.
        normal = -(delta / dist);
        depth = b.radius - dist;
        point = closest; // contact point on a's surface
    }

    return {true, {Thing::nil(), Thing::nil(), normal, point, depth}};
}

/// @brief Sphere vs AABB manifold - symmetric: delegates and negates the normal.
inline ManifoldResult compute_manifold(const Sphere &a, const AABB &b) noexcept {
    ManifoldResult r = compute_manifold(b, a);
    r.manifold.normal = -r.manifold.normal;
    return r;
}

/**
 * @brief Dispatches manifold computation between two `ColliderPart`s based on their kind.
 * @note Covers AABB/AABB, Sphere/Sphere, AABB/Sphere, Sphere/AABB.
 */
inline ManifoldResult compute_manifold(const ColliderPart &a, const ColliderPart &b) noexcept {
    if (a.kind == ColliderKind::AABB && b.kind == ColliderKind::AABB) {
        return compute_manifold(a.aabb, b.aabb);
    }

    if (a.kind == ColliderKind::Sphere && b.kind == ColliderKind::Sphere) {
        return compute_manifold(a.sphere, b.sphere);
    }

    if (a.kind == ColliderKind::AABB && b.kind == ColliderKind::Sphere) {
        return compute_manifold(a.aabb, b.sphere);
    }

    /* Sphere vs AABB */
    return compute_manifold(a.sphere, b.aabb);
}

} // namespace fr
