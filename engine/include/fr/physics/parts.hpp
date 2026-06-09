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
struct LocalTransform {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    FR_SHAPE({
        FR_PROP(pos);
        FR_PROP(rot);
        FR_PROP(scale);
    })

    /// @brief Returns a transform at the origin with identity rotation and unit scale.
    static LocalTransform identity() noexcept {
        return {};
    }

    /// @brief Returns a transform at `p` with identity rotation and unit scale.
    static LocalTransform from_pos(Vec3 p) noexcept {
        LocalTransform t;
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
struct WorldTransform {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    Mat4 mat{1.0f};

    FR_SHAPE({
        FR_PROP(pos);
        FR_PROP(rot);
        FR_PROP(mat);
    })

    /// @brief Returns an identity world transform.
    static WorldTransform identity() noexcept {
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
struct Mass {
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
    static Mass from_mass(F32 m, F32 restitution = 0.5f) noexcept {
        FR_ASSERT(m > 0.0f, "mass must be positive");
        return {Mat3(1.0f / m), 1.0f / m, restitution};
    }

    /// @brief Creates an immovable body (inv_mass = 0, zero inertia).
    static Mass infinite() noexcept {
        return {Mat3(0.0f), 0.0f, 0.0f};
    }

    /// @brief Solid sphere: I = (2/5) * m * r^2 on each diagonal axis.
    static Mass from_sphere(F32 m, F32 radius, F32 restitution = 0.5f) noexcept {
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
    static Mass from_box(F32 m, Vec3 half_extents, F32 restitution = 0.5f) noexcept {
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

    /// @brief Returns true if @p point is inside or on the boundary.
    bool contains(const Vec3 &point) const noexcept {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    /// @brief Returns true if this box overlaps @p other (touching counts as overlap).
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
struct Collider {
    ColliderKind kind{ColliderKind::AABB};
    union {
        AABB aabb;
        Sphere sphere;
    };

    Vec3 offset{};

    Collider() noexcept {

    };

    /// @brief Creates an AABB collider with an optional local-space offset.
    static Collider make_aabb(AABB box, Vec3 offset = {}) noexcept {
        Collider c;
        c.kind = ColliderKind::AABB;
        c.aabb = box;
        c.offset = offset;

        return c;
    }

    /// @brief Creates a sphere collider with an optional local-space offset.
    static Collider make_sphere(Sphere s, Vec3 offset = {}) noexcept {
        Collider c;
        c.kind = ColliderKind::Sphere;
        c.sphere = s;
        c.offset = offset;

        return c;
    }
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
struct CollisionEvents {
    static constexpr USize MAX_COLLISIONS = 16;

    USize count{0};
    Array<Thing, MAX_COLLISIONS> contacts{
        Array<Thing, MAX_COLLISIONS>::from_repeated(Thing::nil())};

    /// @brief Returns true if @p thing appears in the current contact list.
    bool has(Thing thing) const noexcept {
        for (USize i = 0; i < count; ++i) {
            if (contacts[i] == thing) {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Inserts @p thing into the contact list if there is room.
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

// ================================================================ Physics Material

/**
 * @brief Surface response parameters for collision resolution.
 *
 * @details
 * When two bodies collide, the solver typically combines their materials
 * (e.g. by averaging or taking the minimum) to compute a single friction and
 * restitution value for the contact.
 */
struct PhysicsMaterial {
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
inline bool check_collision(const Collider &a, const Collider &b) noexcept {
    if (a.kind == ColliderKind::AABB && b.kind == ColliderKind::AABB)
        return check_collision(a.aabb, b.aabb);
    if (a.kind == ColliderKind::Sphere && b.kind == ColliderKind::Sphere)
        return check_collision(a.sphere, b.sphere);
    if (a.kind == ColliderKind::AABB && b.kind == ColliderKind::Sphere)
        return check_collision(a.aabb, b.sphere);
    /* Sphere vs AABB */ return check_collision(b.aabb, a.sphere);
}

} // namespace fr
