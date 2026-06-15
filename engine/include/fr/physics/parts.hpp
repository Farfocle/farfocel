/**
 * @file parts.hpp
 * @author Kiju
 *
 * @brief Collection of physics parts for the physics engine.
 * @note Coordinate convention: right-handed, Y-up, consistent with GLM defaults.
 */

#pragma once

#include "fr/core/array.hpp"
#include "fr/core/math.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// ======================================================================== Mass

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

    /**
     * @brief Center of mass in **local body space** (metres from the body origin).
     *
     * @details
     * Used by the constraint solver to find the moment arm from the COM to the
     * contact point, which determines the torque applied by each impulse.
     * For geometrically symmetric bodies whose mesh origin coincides with the
     * COM this is `{0, 0, 0}`.
     */
    Vec3 com{0.0f, 0.0f, 0.0f};

    FR_SHAPE({
        FR_PROP(inv_inertia);
        FR_PROP(inv_mass);
        FR_PROP(restitution);
        FR_PROP(com);
    })

    /// @brief Returns the actual mass value (0 if this is a static / infinite-mass body).
    F32 mass() const noexcept;

    /// @brief Creates a mass with a scalar `m` and identity (isotropic) inertia tensor.
    static MassPart from_mass(F32 m, F32 restitution = 0.5f) noexcept;

    /// @brief Creates an immovable body (inv_mass = 0, zero inertia).
    static MassPart infinite() noexcept;

    /// @brief Solid sphere: I = (2/5) * m * r^2 on each diagonal axis.
    static MassPart from_sphere(F32 m, F32 radius, F32 restitution = 0.5f) noexcept;

    /**
     * @brief Solid box: Ix = (1/3)*m*(hy^2+hz^2), Iy = (1/3)*m*(hx^2+hz^2), Iz =
     * (1/3)*m*(hx^2+hy^2).
     *
     * @param half_extents Half-dimensions along each axis (hx, hy, hz).
     */
    static MassPart from_box(F32 m, Vec3 half_extents, F32 restitution = 0.5f) noexcept;
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
    Vec3 center() const noexcept;

    /// @brief Returns the half-extents (half the size along each axis).
    Vec3 half_extents() const noexcept;

    /// @brief Returns true if `point` is inside or on the boundary.
    bool contains(const Vec3 &point) const noexcept;

    /// @brief Returns true if this box overlaps `other` (touching counts as overlap).
    bool overlaps(const AABB &other) const noexcept;

    /// @brief Creates an AABB from a center point and half-extents.
    static AABB from_center(Vec3 center, Vec3 half_extents) noexcept;
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
    bool contains(const Vec3 &point) const noexcept;

    /// @brief Returns true if this sphere overlaps `other`.
    bool overlaps(const Sphere &other) const noexcept;
};

// ============================================================= Math Utilities

/// @brief Overlap length on 1-D intervals [min_a, max_a] and [min_b, max_b].
/// Positive = overlapping; zero or negative = separated.
F32 interval_overlap(F32 min_a, F32 max_a, F32 min_b, F32 max_b) noexcept;

/// @brief Midpoint of the overlap region of two 1-D intervals.
/// @pre Intervals must overlap (interval_overlap > 0).
F32 interval_overlap_center(F32 min_a, F32 max_a, F32 min_b, F32 max_b) noexcept;

/// @brief Closest point on `aabb` to `point`.
Vec3 closest_point_on_aabb(const AABB &aabb, Vec3 point) noexcept;

/// @brief Normalizes `v`; returns `fallback` when the magnitude is near zero.
Vec3 safe_normalize(Vec3 v, Vec3 fallback = {0.0f, 1.0f, 0.0f}) noexcept;

/// @brief Tightest AABB enclosing `s`.
AABB sphere_aabb(const Sphere &s) noexcept;

/// @brief Smallest AABB containing both `a` and `b`.
AABB aabb_union(const AABB &a, const AABB &b) noexcept;

/// @brief Outward normal and distance to the nearest face of `aabb` from an interior `point`.
/// @pre `point` must be inside `aabb`.
struct NearestFace {
    Vec3 normal;
    F32 dist;
};

NearestFace nearest_aabb_face(const AABB &aabb, Vec3 point) noexcept;

// ==================================================================== Collider

/// @brief Identifies which collision primitive is active inside a `Collider`.
enum class ColliderKind : U8 { AABB, Sphere };

template <typename Archive>
void shape(Archive &a, ColliderKind &value) {
    if constexpr (Archive::action == ArchiveAction::Write) {
        const char *str = value == ColliderKind::AABB ? "aabb" : "sphere";
        a.prop("@value", str);
    } else {
        StringView str;
        a.prop("@value", str);
        value = (str == "sphere") ? ColliderKind::Sphere : ColliderKind::AABB;
    }
}

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

    ColliderPart() noexcept {};

    FR_SHAPE({
        FR_PROP(kind);
        FR_PROP(aabb);
        FR_PROP(sphere);
        FR_PROP(offset);
    })

    /// @brief Creates an AABB collider with an optional local-space offset.
    static ColliderPart make_aabb(AABB box, Vec3 offset = {}) noexcept;

    /// @brief Creates a sphere collider with an optional local-space offset.
    static ColliderPart make_sphere(Sphere s, Vec3 offset = {}) noexcept;
};

// ============================================================ Compound Collider

/**
 * @brief A collider composed of up to MAX_SHAPES primitive sub-shapes.
 *
 * @details
 * Each sub-shape is a `ColliderPart` (AABB or Sphere) with its own local-space offset.
 * Use `bounds()` to get the enclosing union AABB for broad-phase queries.
 * The narrow-phase tests all sub-shape pairs and picks the deepest penetration manifold.
 */
struct CompoundColliderPart {
    static constexpr USize MAX_SHAPES = 8;

    Array<ColliderPart, MAX_SHAPES> shapes{};
    USize count{0};

    FR_SHAPE({
        FR_PROP(count);
        FR_PROP(shapes);
    })

    /// @brief Appends a sub-shape. Returns false if already at capacity.
    bool add(ColliderPart shape) noexcept;
    bool add_aabb(AABB box, Vec3 offset = {}) noexcept;
    bool add_sphere(Sphere s, Vec3 offset = {}) noexcept;

    /// @brief Union AABB of all sub-shapes in local space. Used for broad-phase.
    AABB bounds() const noexcept;
};

// ================================================================== Manifolds

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

    FR_SHAPE({
        FR_PROP(count);
        FR_PROP(contacts);
    })

    /// @brief Returns true if `thing` appears in the current contact list.
    bool has(Thing thing) const noexcept;

    /**
     * @brief Inserts `thing` into the contact list if there is room.
     * @return True if the contact was inserted, false if the list is full.
     */
    bool insert(Thing thing) noexcept;
};

// ============================================================ Physics Material

/**
 * @brief Surface response parameters for collision resolution.
 *
 * @details
 * When two bodies collide, the solver typically combines their materials to compute a single
 * friction and restitution value for the contact.
 */
struct PhysicsMaterialPart {
    F32 friction{0.5f};
    F32 restitution{0.5f};

    FR_SHAPE({
        FR_PROP(friction);
        FR_PROP(restitution);
    })
};

// ================================================================== Rigid Body

/**
 * @brief Dynamics state for a rigid body.
 *
 * @details
 * Pair this part with a `MassPart` (which carries `inv_inertia` and the `com` offset)
 * to get rotational dynamics. Without `MassPart` the body still moves linearly but
 * never accumulates angular velocity.
 *
 * Bodies with `inv_mass == 0` are immovable static bodies - all force, impulse, and
 * integration steps are no-ops for them.
 */
struct RigidBodyPart {
    /// @brief Linear velocity in world space (m/s).
    Vec3 velocity{0.0f, 0.0f, 0.0f};

    /// @brief Angular velocity in world space (rad/s).
    Vec3 angular_velocity{0.0f, 0.0f, 0.0f};

    /// @brief Accumulated external force for this frame (N). Cleared after integration.
    Vec3 force{0.0f, 0.0f, 0.0f};

    /// @brief Accumulated external torque for this frame (N·m). Cleared after integration.
    Vec3 torque{0.0f, 0.0f, 0.0f};

    /// @brief Inverse mass (kg⁻¹). Set to 0 for a static / infinite-mass body.
    F32 inv_mass{1.0f};

    /**
     * @brief Coefficient of restitution (bounciness), in [0, 1].
     * 0 = perfectly inelastic, 1 = perfectly elastic.
     */
    F32 restitution{0.3f};

    /**
     * @brief Coefficient of friction (Coulomb model), in [0, 1].
     * The solver uses the geometric mean of the two colliding bodies.
     */
    F32 friction{0.5f};

    FR_SHAPE({
        FR_PROP(velocity);
        FR_PROP(angular_velocity);
        FR_PROP(force);
        FR_PROP(torque);
        FR_PROP(inv_mass);
        FR_PROP(restitution);
        FR_PROP(friction);
    })

    /// @brief Returns the actual mass in kg (0 if this is a static body).
    F32 mass() const noexcept;

    /// @brief Creates a dynamic body with the given mass (kg) and optional surface properties.
    static RigidBodyPart make_dynamic(F32 mass, F32 restitution = 0.3f,
                                      F32 friction = 0.5f) noexcept;

    /// @brief Creates an immovable static body (inv_mass = 0).
    static RigidBodyPart make_static(F32 restitution = 0.3f, F32 friction = 0.5f) noexcept;
};

// ============================================================= Collision Tests

bool check_collision(const AABB &a, const AABB &b) noexcept;
bool check_collision(const Sphere &a, const Sphere &b) noexcept;
bool check_collision(const AABB &a, const Sphere &s) noexcept;
bool check_collision(const Sphere &s, const AABB &a) noexcept;
bool check_collision(const ColliderPart &a, const ColliderPart &b) noexcept;
bool check_collision(const CompoundColliderPart &a, const ColliderPart &b) noexcept;
bool check_collision(const ColliderPart &a, const CompoundColliderPart &b) noexcept;
bool check_collision(const CompoundColliderPart &a, const CompoundColliderPart &b) noexcept;

// ======================================================== Manifold Computation

/**
 * @brief Result of a narrow-phase collision test.
 *
 * @details If `hit` is false, the `manifold` field is uninitialised and must not be read.
 * The `Thing` fields inside `manifold` are left as `Thing::nil()` - the caller (narrowphase
 * system) is responsible for filling them in before storing the manifold.
 */
struct CollisionManifoldResult {
    bool hit{false};
    CollisionManifold manifold{};
};

CollisionManifoldResult compute_manifold(const AABB &a, const AABB &b) noexcept;
CollisionManifoldResult compute_manifold(const Sphere &a, const Sphere &b) noexcept;
CollisionManifoldResult compute_manifold(const AABB &a, const Sphere &b) noexcept;
CollisionManifoldResult compute_manifold(const Sphere &a, const AABB &b) noexcept;
CollisionManifoldResult compute_manifold(const ColliderPart &a, const ColliderPart &b) noexcept;
CollisionManifoldResult compute_manifold(const CompoundColliderPart &a,
                                         const ColliderPart &b) noexcept;
CollisionManifoldResult compute_manifold(const ColliderPart &a,
                                         const CompoundColliderPart &b) noexcept;
CollisionManifoldResult compute_manifold(const CompoundColliderPart &a,
                                         const CompoundColliderPart &b) noexcept;

} // namespace fr

FR_TYPE(fr::MassPart);
FR_TYPE(fr::ColliderPart);
FR_TYPE(fr::CompoundColliderPart);
FR_TYPE(fr::CollisionEventsPart);
FR_TYPE(fr::PhysicsMaterialPart);
FR_TYPE(fr::RigidBodyPart);
