/**
 * @file spawn_actions.hpp
 * @brief Scene-level thing spawn helpers.
 *
 * @details
 * All spawn functions take a Scope, return a Thing, and use deferred inserts.
 * Transform rebuilds and asset resolution happen at end-of-frame via scheduled systems.
 */

#pragma once

#include "fr/core/string_view.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr {

/// @brief Returns a LocalTransformPart placed `distance` units in front of the main camera.
/// Falls back to origin if no main camera exists in the world.
inline LocalTransformPart spawn_in_front_of_camera(World &world, F32 distance = 5.0f) noexcept {
    for (auto [thing, cam, trans] : world.query<CameraPart, WorldTransformPart>()) {
        (void)thing;
        if (!cam.is_main) {
            continue;
        }
        const Vec3 forward = trans.rotation * Vec3(0.0f, 0.0f, -1.0f);
        LocalTransformPart t{};
        t.position = trans.position + forward * distance;
        return t;
    }
    return {};
}

/// @brief Spawns an empty thing with relations and local transform parts (deferred).
Thing spawn_base(Scope scope, const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a mesh renderer thing referencing a cooked logical .fmesh path (deferred).
Thing spawn_mesh(Scope scope, StringView mesh_path,
                 const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a camera thing with FPS controller (deferred).
Thing spawn_camera(Scope scope, const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a directional light thing (deferred).
Thing spawn_directional_light(Scope scope, const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a point light thing (deferred).
Thing spawn_point_light(Scope scope, const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a spot light thing (deferred).
Thing spawn_spot_light(Scope scope, const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a primitive mesh thing (deferred).
Thing spawn_primitive(Scope scope, PrimitiveMeshKind kind,
                      const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a cube primitive (deferred).
Thing spawn_cube(Scope scope, const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a plane primitive (deferred).
Thing spawn_plane(Scope scope, const LocalTransformPart &local = {}) noexcept;

/// @brief Spawns a grid primitive (deferred).
Thing spawn_grid(Scope scope, const LocalTransformPart &local = {}) noexcept;

} // namespace fr
