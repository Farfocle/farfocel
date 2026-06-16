/**
 * @file transform_system.hpp
 * @author Tfoedy
 * @brief ECS transform propagation system.
 */

#pragma once

#include <glm/gtc/quaternion.hpp>

#include "fr/core/math.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"

namespace fr {

/// @brief Composes a world-space transform from a local transform and an optional parent.
/// If parent is null the local transform is treated as already world-space.
[[nodiscard]] inline WorldTransformPart
compose_world_transform(const LocalTransformPart &local,
                        const WorldTransformPart *parent) noexcept {
    WorldTransformPart out{};
    const Mat4 local_matrix = local.to_mat4();

    if (!parent) {
        out.position = local.position;
        out.rotation = glm::normalize(local.rotation);
        out.scale = local.scale;
        out.matrix = local_matrix;
        return out;
    }

    out.matrix = parent->matrix * local_matrix;
    out.position = Vec3(out.matrix[3].x, out.matrix[3].y, out.matrix[3].z);
    out.rotation = glm::normalize(parent->rotation * local.rotation);
    out.scale = parent->scale * local.scale;
    return out;
}

/// @brief Inserts a WorldTransformPart for every thing that has `LocalTransformPart` but lacks one.
inline void ensure_world_transform_pairs_now(World &world) noexcept {
    for (auto [thing, local] : world.query<LocalTransformPart>()) {
        if (world.has<WorldTransformPart>(thing)) {
            continue;
        }

        WorldTransformPart wt = compose_world_transform(local, nullptr);
        world.insert_now<WorldTransformPart>(thing, wt);
    }
}

/**
 * @brief Rebuilds all WorldTransformParts from LocalTransformParts, parent-first.
 * @note Must be called whenever local transforms or hierarchy relationships change.
 */
inline void rebuild_world_transforms(World &world) noexcept {
    world.ensure<LocalTransformPart>();
    world.ensure<WorldTransformPart>();
    world.ensure<RelationsPart>();

    ensure_world_transform_pairs_now(world);
    world.sort_by_hierarchy_depth<LocalTransformPart>();

    for (auto [thing, local] : world.query<LocalTransformPart>()) {
        const RelationsPart *relations = world.try_get<RelationsPart>(thing);

        const WorldTransformPart *parent_world = nullptr;
        if (relations && !relations->parent.is_nil() &&
            world.has<WorldTransformPart>(relations->parent)) {
            parent_world = world.try_get<WorldTransformPart>(relations->parent);
        }

        WorldTransformPart *wt = world.try_get<WorldTransformPart>(thing);
        if (wt) {
            *wt = compose_world_transform(local, parent_world);
        }
    }
}

} // namespace fr
