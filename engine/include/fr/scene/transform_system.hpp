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

/**
 * @brief Rebuilds cached world transforms from local transforms and hierarchy data.
 *
 * @details
 * LocalTransformPart is treated as persistent scene data. WorldTransformPart is treated as a
 * runtime cache consumed by renderer, cameras, picking and physics-like queries.
 */
class TransformSystem {
public:
    /**
     * @brief Rebuilds all WorldTransformPart values from LocalTransformPart.
     *
     * @details
     * The function guarantees that every entity with LocalTransformPart also has a
     * WorldTransformPart. Entities are processed parent-first by sorting the LocalTransformPart
     * pool using hierarchy depth.
     */
    static void rebuild_world_transforms(World &world) noexcept {
        world.ensure<LocalTransformPart>();
        world.ensure<WorldTransformPart>();
        world.ensure<RelationsPart>();

        ensure_world_transform_parts(world);

        world.sort_by_hierarchy_depth<LocalTransformPart>();

        for (auto [thing, local] : world.query<LocalTransformPart>()) {
            const RelationsPart *relations = world.try_get<RelationsPart>(thing);

            const WorldTransformPart *parent_world = nullptr;
            if (relations && !relations->parent.is_nil() &&
                world.has<WorldTransformPart>(relations->parent)) {
                parent_world = world.try_get<WorldTransformPart>(relations->parent);
            }

            WorldTransformPart *world_transform = world.try_get<WorldTransformPart>(thing);
            if (!world_transform) {
                continue;
            }

            *world_transform = compose_world_transform(local, parent_world);
        }
    }

    /**
     * @brief Composes one world transform from a local transform and optional parent transform.
     */
    [[nodiscard]] static WorldTransformPart
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

private:
    /**
     * @brief Ensures every local transform has a matching world transform cache.
     */
    static void ensure_world_transform_parts(World &world) noexcept {
        for (auto [thing, local] : world.query<LocalTransformPart>()) {
            if (world.has<WorldTransformPart>(thing)) {
                continue;
            }

            WorldTransformPart world_transform = compose_world_transform(local, nullptr);
            world.insert_now<WorldTransformPart>(thing, world_transform);
        }
    }
};

} // namespace fr
