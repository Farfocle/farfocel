/**
 * @file picking.hpp
 * @brief Scene-level ray-casting and object picking helpers.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/core/math.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"

namespace fr {

struct PickingRay {
    Vec3 origin{0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};

    FR_SHAPE({
        FR_PROP(origin);
        FR_PROP(direction);
    })
};

struct PickingHit {
    Thing thing{Thing::nil()};
    Vec3 position{0.0f};
    F32 distance{0.0f};

    static PickingHit nil() noexcept {
        return PickingHit{Thing::nil(), {0.0f, 0.0f, 0.0f}, 0.0f};
    }

    bool is_nil() const noexcept {
        return thing.is_nil();
    }

    [[nodiscard]] bool is_valid() const noexcept {
        return !thing.is_nil();
    }

    FR_SHAPE({
        FR_PROP(thing);
        FR_PROP(position);
        FR_PROP(distance);
    })
};

struct CameraMatrices {
    Mat4 view{1.0f};
    Mat4 projection{1.0f};
    Mat4 view_projection{1.0f};

    Vec3 position{0.0f};
    Vec3 forward{0.0f, 0.0f, -1.0f};

    bool found{false};

    FR_SHAPE({
        FR_PROP(view);
        FR_PROP(projection);
        FR_PROP(view_projection);
        FR_PROP(position);
        FR_PROP(forward);
        FR_PROP(found);
    })
};

CameraMatrices extract_camera_matrices(World &world, F32 aspect_ratio) noexcept;

PickingRay make_picking_ray(F32 mouse_x, F32 mouse_y, F32 viewport_width, F32 viewport_height,
                            const CameraMatrices &camera) noexcept;

bool check_ray_aabb(const PickingRay &ray, const Vec3 &aabb_min, const Vec3 &aabb_max,
                    F32 &out_distance) noexcept;

PickingRay transform_ray(const PickingRay &ray, const Mat4 &inverse_transform) noexcept;

PickingHit pick_scene_mesh_aabbs(World &world, const AssetManager &assets,
                                 const PickingRay &ray) noexcept;

} // namespace fr
