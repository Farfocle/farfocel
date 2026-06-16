/**
 * @file object_picking.hpp
 * @author Tfoedy
 * @brief Runtime object picking helpers for devtools.
 */

#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "fr/asset/asset_manager.hpp"
#include "fr/core/math.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/renderer/render_mesh.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr::devtools {

struct PickingRay {
    Vec3 origin{0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};
};

struct PickingHit {
    Thing thing{Thing::nil()};
    Vec3 position{0.0f};
    F32 distance{0.0f};

    [[nodiscard]] bool is_valid() const noexcept {
        return !thing.is_nil();
    }
};

struct EditorCameraMatrices {
    Mat4 view{1.0f};
    Mat4 projection{1.0f};
    Mat4 view_projection{1.0f};

    Vec3 position{0.0f};
    Vec3 forward{0.0f, 0.0f, -1.0f};

    bool found{false};
};

inline EditorCameraMatrices extract_editor_camera_matrices(World &world,
                                                           F32 aspect_ratio) noexcept {
    EditorCameraMatrices result{};

    if (aspect_ratio <= 0.0f) {
        aspect_ratio = 1.0f;
    }

    world.for_each_alive_thing([&](Thing thing) noexcept {
        if (result.found || thing.is_nil()) {
            return;
        }

        CameraPart *camera = world.try_get<CameraPart>(thing);
        WorldTransformPart *transform = world.try_get<WorldTransformPart>(thing);

        if (!camera || !transform || !camera->is_main) {
            return;
        }

        const Vec3 forward = transform->rotation * Vec3(0.0f, 0.0f, -1.0f);
        const Vec3 up = transform->rotation * Vec3(0.0f, 1.0f, 0.0f);

        result.projection = glm::perspective(glm::radians(camera->fov), aspect_ratio,
                                             camera->near_plane, camera->far_plane);

        result.view = glm::lookAt(transform->position, transform->position + forward, up);

        result.view_projection = result.projection * result.view;
        result.position = transform->position;
        result.forward = forward;
        result.found = true;
    });

    return result;
}

inline PickingRay make_picking_ray(F32 mouse_x, F32 mouse_y, F32 viewport_width,
                                   F32 viewport_height,
                                   const EditorCameraMatrices &camera) noexcept {
    PickingRay ray{};

    if (!camera.found || viewport_width <= 0.0f || viewport_height <= 0.0f) {
        return ray;
    }

    const F32 ndc_x = (2.0f * mouse_x) / viewport_width - 1.0f;
    const F32 ndc_y = 1.0f - (2.0f * mouse_y) / viewport_height;

    const Mat4 inv_view_projection = glm::inverse(camera.view_projection);

    Vec4 near_point = inv_view_projection * Vec4(ndc_x, ndc_y, -1.0f, 1.0f);
    Vec4 far_point = inv_view_projection * Vec4(ndc_x, ndc_y, 1.0f, 1.0f);

    if (near_point.w != 0.0f) {
        near_point /= near_point.w;
    }

    if (far_point.w != 0.0f) {
        far_point /= far_point.w;
    }

    const Vec3 near_world = Vec3(near_point);
    const Vec3 far_world = Vec3(far_point);

    ray.origin = camera.position;

    Vec3 direction = far_world - near_world;
    if (glm::length(direction) <= 0.0001f) {
        direction = camera.forward;
    }

    ray.direction = glm::normalize(direction);
    return ray;
}

inline bool ray_intersects_aabb(const PickingRay &ray, const Vec3 &aabb_min, const Vec3 &aabb_max,
                                F32 &out_distance) noexcept {
    constexpr F32 EPSILON = 0.000001f;

    F32 t_min = 0.0f;
    F32 t_max = 1.0e30f;

    for (U32 axis = 0; axis < 3; ++axis) {
        const F32 origin = ray.origin[axis];
        const F32 direction = ray.direction[axis];
        const F32 min_v = aabb_min[axis];
        const F32 max_v = aabb_max[axis];

        if (glm::abs(direction) < EPSILON) {
            if (origin < min_v || origin > max_v) {
                return false;
            }

            continue;
        }

        const F32 inv_direction = 1.0f / direction;

        F32 t0 = (min_v - origin) * inv_direction;
        F32 t1 = (max_v - origin) * inv_direction;

        if (t0 > t1) {
            const F32 tmp = t0;
            t0 = t1;
            t1 = tmp;
        }

        t_min = glm::max(t_min, t0);
        t_max = glm::min(t_max, t1);

        if (t_min > t_max) {
            return false;
        }
    }

    if (t_max < 0.0f) {
        return false;
    }

    out_distance = t_min > 0.0f ? t_min : t_max;
    return out_distance > 0.0001f;
}

inline PickingRay transform_ray(const PickingRay &ray, const Mat4 &inverse_transform) noexcept {
    PickingRay out{};

    out.origin = Vec3(inverse_transform * Vec4(ray.origin, 1.0f));

    Vec3 local_direction = Vec3(inverse_transform * Vec4(ray.direction, 0.0f));
    if (glm::length(local_direction) <= 0.0001f) {
        local_direction = Vec3(0.0f, 0.0f, -1.0f);
    }

    out.direction = glm::normalize(local_direction);
    return out;
}

inline PickingHit pick_scene_mesh_aabbs(World &world, const AssetManager &assets,
                                        const PickingRay &ray) noexcept {
    PickingHit best_hit{};
    F32 best_distance = 1.0e30f;

    world.for_each_alive_thing([&](Thing thing) noexcept {
        if (thing.is_nil()) {
            return;
        }

        MeshRendererPart *mesh_part = world.try_get<MeshRendererPart>(thing);
        WorldTransformPart *transform = world.try_get<WorldTransformPart>(thing);

        if (!mesh_part || !transform || !mesh_part->visible || !mesh_part->mesh_handle.is_valid()) {
            return;
        }

        const RenderMeshData *mesh_data = assets.get_mesh_data(mesh_part->mesh_handle);
        if (!mesh_data) {
            return;
        }

        for (USize i = 0; i < mesh_data->submeshes.size(); ++i) {
            const RenderSubMesh &submesh = mesh_data->submeshes[i];

            const Vec3 local_min = submesh.aabb_min;
            const Vec3 local_max = submesh.aabb_max;

            if (local_min.x > local_max.x || local_min.y > local_max.y ||
                local_min.z > local_max.z) {
                continue;
            }

            const Mat4 model = transform->matrix * submesh.transform;
            const Mat4 inverse_model = glm::inverse(model);

            const PickingRay local_ray = transform_ray(ray, inverse_model);

            F32 local_distance = 0.0f;
            if (!ray_intersects_aabb(local_ray, local_min, local_max, local_distance)) {
                continue;
            }

            const Vec3 local_hit = local_ray.origin + local_ray.direction * local_distance;
            const Vec3 world_hit = Vec3(model * Vec4(local_hit, 1.0f));
            const F32 world_distance = glm::length(world_hit - ray.origin);

            if (world_distance < best_distance) {
                best_distance = world_distance;
                best_hit.thing = thing;
                best_hit.distance = world_distance;
                best_hit.position = world_hit;
            }
        }
    });

    return best_hit;
}

} // namespace fr::devtools
