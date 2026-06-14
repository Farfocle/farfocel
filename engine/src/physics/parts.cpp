/**
 * @file parts.cpp
 * @author Kiju
 *
 * @brief Physics parts implementation: math utilities, collision tests, manifold computation.
 */

#include "fr/physics/parts.hpp"

#include <cmath>

#include "glm/common.hpp"
#include "glm/geometric.hpp"

namespace fr {

// ======================================================================== Mass

F32 MassPart::mass() const noexcept {
    return inv_mass > 0.0f ? 1.0f / inv_mass : 0.0f;
}

MassPart MassPart::from_mass(F32 m, F32 restitution) noexcept {
    FR_ASSERT(m > 0.0f, "mass must be positive");
    return {Mat3(1.0f / m), 1.0f / m, restitution};
}

MassPart MassPart::infinite() noexcept {
    return {Mat3(0.0f), 0.0f, 0.0f};
}

MassPart MassPart::from_sphere(F32 m, F32 radius, F32 restitution) noexcept {
    FR_ASSERT(m > 0.0f && radius > 0.0f, "mass and radius must be positive");

    const F32 i_inv = 5.0f / (2.0f * m * radius * radius);
    return {Mat3(i_inv), 1.0f / m, restitution};
}

MassPart MassPart::from_box(F32 m, Vec3 half_extents, F32 restitution) noexcept {
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

// ========================================================= Broad-Phase Volumes

Vec3 AABB::center() const noexcept {
    return (min + max) * 0.5f;
}

Vec3 AABB::half_extents() const noexcept {
    return (max - min) * 0.5f;
}

bool AABB::contains(const Vec3 &point) const noexcept {
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

bool AABB::overlaps(const AABB &other) const noexcept {
    return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y &&
           max.y >= other.min.y && min.z <= other.max.z && max.z >= other.min.z;
}

AABB AABB::from_center(Vec3 c, Vec3 he) noexcept {
    return {c - he, c + he};
}

bool Sphere::contains(const Vec3 &point) const noexcept {
    const Vec3 offset = point - center;
    return glm::dot(offset, offset) <= radius * radius;
}

bool Sphere::overlaps(const Sphere &other) const noexcept {
    const F32 combined = radius + other.radius;
    return glm::distance(center, other.center) <= combined;
}

// ============================================================== Math Utilities

F32 interval_overlap(F32 min_a, F32 max_a, F32 min_b, F32 max_b) noexcept {
    return std::min(max_a, max_b) - std::max(min_a, min_b);
}

F32 interval_overlap_center(F32 min_a, F32 max_a, F32 min_b, F32 max_b) noexcept {
    return (std::max(min_a, min_b) + std::min(max_a, max_b)) * 0.5f;
}

Vec3 closest_point_on_aabb(const AABB &aabb, Vec3 point) noexcept {
    return glm::clamp(point, aabb.min, aabb.max);
}

Vec3 safe_normalize(Vec3 v, Vec3 fallback) noexcept {
    const F32 len_sq = glm::dot(v, v);
    return len_sq > 1e-12f ? v / std::sqrt(len_sq) : fallback;
}

AABB sphere_aabb(const Sphere &s) noexcept {
    return {s.center - s.radius, s.center + s.radius};
}

AABB aabb_union(const AABB &a, const AABB &b) noexcept {
    return {glm::min(a.min, b.min), glm::max(a.max, b.max)};
}

NearestFace nearest_aabb_face(const AABB &aabb, Vec3 point) noexcept {
    const Vec3 d_min = point - aabb.min;
    const Vec3 d_max = aabb.max - point;

    NearestFace best{{-1.0f, 0.0f, 0.0f}, d_min.x};

    auto consider = [&](Vec3 n, F32 d) noexcept {
        if (d < best.dist) {
            best = {n, d};
        }
    };

    consider({0.0f, -1.0f, 0.0f}, d_min.y);
    consider({0.0f, 0.0f, -1.0f}, d_min.z);
    consider({1.0f, 0.0f, 0.0f}, d_max.x);
    consider({0.0f, 1.0f, 0.0f}, d_max.y);
    consider({0.0f, 0.0f, 1.0f}, d_max.z);

    return best;
}

// ==================================================================== Collider

ColliderPart ColliderPart::make_aabb(AABB box, Vec3 offset) noexcept {
    ColliderPart c;
    c.kind = ColliderKind::AABB;
    c.aabb = box;
    c.offset = offset;
    return c;
}

ColliderPart ColliderPart::make_sphere(Sphere s, Vec3 offset) noexcept {
    ColliderPart c;
    c.kind = ColliderKind::Sphere;
    c.sphere = s;
    c.offset = offset;

    return c;
}

// ============================================================ Compound Collider

bool CompoundColliderPart::add(ColliderPart shape) noexcept {
    if (count >= MAX_SHAPES) {
        return false;
    }

    shapes[count++] = shape;
    return true;
}

bool CompoundColliderPart::add_aabb(AABB box, Vec3 offset) noexcept {
    return add(ColliderPart::make_aabb(box, offset));
}

bool CompoundColliderPart::add_sphere(Sphere s, Vec3 offset) noexcept {
    return add(ColliderPart::make_sphere(s, offset));
}

AABB CompoundColliderPart::bounds() const noexcept {
    if (count == 0) {
        return {};
    }
    auto shape_aabb = [](const ColliderPart &c) noexcept -> AABB {
        if (c.kind == ColliderKind::AABB) {
            return {c.aabb.min + c.offset, c.aabb.max + c.offset};
        }

        return sphere_aabb({c.sphere.center + c.offset, c.sphere.radius});
    };

    AABB result = shape_aabb(shapes[0]);

    for (USize i = 1; i < count; ++i) {
        result = aabb_union(result, shape_aabb(shapes[i]));
    }

    return result;
}

// ============================================================ Collision Events

bool CollisionEventsPart::has(Thing thing) const noexcept {
    for (USize i = 0; i < count; ++i) {
        if (contacts[i] == thing) {
            return true;
        }
    }

    return false;
}

bool CollisionEventsPart::insert(Thing thing) noexcept {
    if (count < MAX_COLLISIONS) {
        contacts[count++] = thing;
        return true;
    }

    return false;
}

// ================================================================== Rigid Body

F32 RigidBodyPart::mass() const noexcept {
    return inv_mass > 0.0f ? 1.0f / inv_mass : 0.0f;
}

RigidBodyPart RigidBodyPart::make_dynamic(F32 mass, F32 restitution, F32 friction) noexcept {
    FR_ASSERT(mass > 0.0f, "mass must be positive");

    RigidBodyPart rb;
    rb.inv_mass = 1.0f / mass;
    rb.restitution = restitution;
    rb.friction = friction;

    return rb;
}

RigidBodyPart RigidBodyPart::make_static(F32 restitution, F32 friction) noexcept {
    RigidBodyPart rb;
    rb.inv_mass = 0.0f;
    rb.restitution = restitution;
    rb.friction = friction;

    return rb;
}

// ============================================================= Collision Tests

bool check_collision(const AABB &a, const AABB &b) noexcept {
    return a.overlaps(b);
}

bool check_collision(const Sphere &a, const Sphere &b) noexcept {
    return a.overlaps(b);
}

bool check_collision(const AABB &a, const Sphere &s) noexcept {
    const Vec3 delta = s.center - closest_point_on_aabb(a, s.center);
    return glm::dot(delta, delta) <= s.radius * s.radius;
}

bool check_collision(const Sphere &s, const AABB &a) noexcept {
    return check_collision(a, s);
}

bool check_collision(const ColliderPart &a, const ColliderPart &b) noexcept {
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

bool check_collision(const CompoundColliderPart &a, const ColliderPart &b) noexcept {
    for (USize i = 0; i < a.count; ++i) {
        if (check_collision(a.shapes[i], b)) {
            return true;
        }
    }

    return false;
}

bool check_collision(const ColliderPart &a, const CompoundColliderPart &b) noexcept {
    return check_collision(b, a);
}

bool check_collision(const CompoundColliderPart &a, const CompoundColliderPart &b) noexcept {
    for (USize i = 0; i < a.count; ++i) {
        for (USize j = 0; j < b.count; ++j) {
            if (check_collision(a.shapes[i], b.shapes[j])) {
                return true;
            }
        }
    }

    return false;
}

// ======================================================== Manifold Computation

CollisionManifoldResult compute_manifold(const AABB &a, const AABB &b) noexcept {
    const F32 ox = interval_overlap(a.min.x, a.max.x, b.min.x, b.max.x);
    const F32 oy = interval_overlap(a.min.y, a.max.y, b.min.y, b.max.y);
    const F32 oz = interval_overlap(a.min.z, a.max.z, b.min.z, b.max.z);

    if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) {
        return {false, {}};
    }

    Vec3 normal;
    F32 depth;

    if (ox <= oy && ox <= oz) {
        depth = ox;
        normal = {1.0f, 0.0f, 0.0f};
    } else if (oy <= oz) {
        depth = oy;
        normal = {0.0f, 1.0f, 0.0f};
    } else {
        depth = oz;
        normal = {0.0f, 0.0f, 1.0f};
    }

    if (glm::dot(a.center() - b.center(), normal) < 0.0f) {
        normal = -normal;
    }

    const Vec3 point = {
        interval_overlap_center(a.min.x, a.max.x, b.min.x, b.max.x),
        interval_overlap_center(a.min.y, a.max.y, b.min.y, b.max.y),
        interval_overlap_center(a.min.z, a.max.z, b.min.z, b.max.z),
    };

    return {true, {Thing::nil(), Thing::nil(), normal, point, depth}};
}

CollisionManifoldResult compute_manifold(const Sphere &a, const Sphere &b) noexcept {
    const Vec3 delta = a.center - b.center;
    const F32 dist_sq = glm::dot(delta, delta);
    const F32 combined = a.radius + b.radius;

    if (dist_sq > combined * combined) {
        return {false, {}};
    }

    const F32 dist = std::sqrt(dist_sq);
    const Vec3 normal = safe_normalize(delta);
    const F32 depth = combined - dist;
    const Vec3 point = b.center + normal * b.radius;

    return {true, {Thing::nil(), Thing::nil(), normal, point, depth}};
}

CollisionManifoldResult compute_manifold(const AABB &a, const Sphere &b) noexcept {
    const Vec3 closest = closest_point_on_aabb(a, b.center);
    const Vec3 delta = b.center - closest;
    const F32 dist_sq = glm::dot(delta, delta);

    if (dist_sq > b.radius * b.radius) {
        return {false, {}};
    }

    Vec3 normal;
    F32 depth;
    Vec3 point;

    if (dist_sq < 1e-10f) {
        // Sphere centre is inside the AABB.
        const auto [face_normal, face_dist] = nearest_aabb_face(a, b.center);
        normal = face_normal;
        depth = b.radius + face_dist;
        point = b.center;
    } else {
        // delta points from AABB surface toward sphere center; negate for normal from b toward a.
        normal = -safe_normalize(delta);
        depth = b.radius - std::sqrt(dist_sq);
        point = closest;
    }

    return {true, {Thing::nil(), Thing::nil(), normal, point, depth}};
}

CollisionManifoldResult compute_manifold(const Sphere &a, const AABB &b) noexcept {
    CollisionManifoldResult r = compute_manifold(b, a);
    r.manifold.normal = -r.manifold.normal;
    return r;
}

CollisionManifoldResult compute_manifold(const ColliderPart &a, const ColliderPart &b) noexcept {
    if (a.kind == ColliderKind::AABB && b.kind == ColliderKind::AABB) {
        return compute_manifold(a.aabb, b.aabb);
    }

    if (a.kind == ColliderKind::Sphere && b.kind == ColliderKind::Sphere) {
        return compute_manifold(a.sphere, b.sphere);
    }

    if (a.kind == ColliderKind::AABB && b.kind == ColliderKind::Sphere) {
        return compute_manifold(a.aabb, b.sphere);
    }

    return compute_manifold(a.sphere, b.aabb);
}

CollisionManifoldResult compute_manifold(const CompoundColliderPart &a,
                                         const ColliderPart &b) noexcept {
    CollisionManifoldResult best{false, {}};
    for (USize i = 0; i < a.count; ++i) {
        const CollisionManifoldResult r = compute_manifold(a.shapes[i], b);
        if (r.hit && (!best.hit || r.manifold.depth > best.manifold.depth)) {
            best = r;
        }
    }

    return best;
}

CollisionManifoldResult compute_manifold(const ColliderPart &a,
                                         const CompoundColliderPart &b) noexcept {
    CollisionManifoldResult r = compute_manifold(b, a);
    if (r.hit) {
        r.manifold.normal = -r.manifold.normal;
    }

    return r;
}

CollisionManifoldResult compute_manifold(const CompoundColliderPart &a,
                                         const CompoundColliderPart &b) noexcept {
    CollisionManifoldResult best{false, {}};
    for (USize i = 0; i < a.count; ++i) {
        for (USize j = 0; j < b.count; ++j) {
            const CollisionManifoldResult r = compute_manifold(a.shapes[i], b.shapes[j]);
            if (r.hit && (!best.hit || r.manifold.depth > best.manifold.depth)) {
                best = r;
            }
        }
    }

    return best;
}

} // namespace fr
