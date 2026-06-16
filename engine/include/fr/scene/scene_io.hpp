/**
 * @file scene_io.hpp
 * @brief Scene save/load helpers.
 */

#pragma once

#include <utility>

#include "fr/asset/asset_kind.hpp"
#include "fr/asset/asset_manager.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/file.hpp"
#include "fr/core/json.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/logger/logger.hpp"
#include "fr/scene/environment.hpp"
#include "fr/scene/primitive_mesh.hpp"
#include "fr/scene/render_assets.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/transform.hpp"

namespace fr::scene {

/// @brief Ensures built-in part pools required by a standard Farfocel scene.
inline void ensure_scene_parts(World &world) noexcept {
    world.ensure<RelationsPart>();
    world.ensure<LocalTransformPart>();
    world.ensure<WorldTransformPart>();

    world.ensure<CameraPart>();
    world.ensure<FPSControllerPart>();

    world.ensure<PrimitiveMeshPart>();
    world.ensure<MeshRendererPart>();
    world.ensure<MaterialOverridePart>();

    world.ensure<PointLightPart>();
    world.ensure<SpotLightPart>();
    world.ensure<DirectionalLightPart>();
}

/// @brief Saves persistent scene data to JSON.
inline bool save_scene(World &world, StringView output_path) noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[SceneIO] Cannot save scene to an empty path.");
        return false;
    }

    JsonWriterArchive writer({.pretty = true});
    world.shape_scene(writer);

    if (EnvironmentState *env = world.try_get_resource<EnvironmentState>()) {
        writer.prop("environment", *env);
    }

    String json = writer.consume();
    if (json.size() == 0) {
        FR_LOG_ERR("[SceneIO] Failed to serialize scene: {}", output_path);
        return false;
    }

    String path = String::from_view(output_path);

    if (!file::ensure_parent_directory(path.view())) {
        FR_LOG_ERR("[SceneIO] Failed to create scene output directory: {}", output_path);
        return false;
    }

    const Slice<const Byte> bytes(reinterpret_cast<const Byte *>(json.data()), json.size());

    if (!file::write_all_bytes(path, bytes)) {
        FR_LOG_ERR("[SceneIO] Failed to write scene file: {}", output_path);
        return false;
    }

    FR_LOG("[SceneIO] Saved scene: {}", output_path);
    return true;
}

/**
 * @brief Replaces the current scene with data loaded from a JSON file.
 * @note Releases existing render assets, clears the scene, deserializes, and resolves all systems.
 * If registry is provided, any mesh/material paths found in the scene that are not yet in the
 * registry are auto-registered as loose assets so they can be resolved immediately.
 */
inline bool load_scene(World &world, AssetManager &assets, StringView input_path,
                       AssetRegistry *registry = nullptr) noexcept {
    if (input_path.is_empty()) {
        FR_LOG_ERR("[SceneIO] Cannot load scene from an empty path.");
        return false;
    }

    String path = String::from_view(input_path);

    auto text = file::read_all_text(path);
    if (!text.is_some()) {
        FR_LOG_ERR("[SceneIO] Failed to read scene file: {}", input_path);
        return false;
    }

    release_render_assets(world, assets);
    world.clear_scene();

    ensure_scene_parts(world);

    String json = std::move(text.unwrap());

    JsonReaderArchive reader(json.view());
    world.shape_scene(reader);

    if (EnvironmentState *env = world.try_get_resource<EnvironmentState>()) {
        *env = EnvironmentState{};
        reader.prop("environment", *env);
    }

    if (!reader.consume()) {
        FR_LOG_ERR("[SceneIO] Failed to deserialize scene: {}", input_path);
        return false;
    }

    if (registry) {
        for (auto [thing, mesh] : world.query<MeshRendererPart>()) {
            (void)thing;
            if (mesh.mesh_path.size() == 0) {
                continue;
            }
            const AssetId id = AssetId::from_logical_path(mesh.mesh_path.view());
            if (!registry->find(id)) {
                registry->register_loose_asset(id, AssetKind::Mesh, mesh.mesh_path.view());
            }
        }
        for (auto [thing, mat] : world.query<MaterialOverridePart>()) {
            (void)thing;
            if (mat.material_path.size() == 0) {
                continue;
            }
            const AssetId id = AssetId::from_logical_path(mat.material_path.view());
            if (!registry->find(id)) {
                registry->register_loose_asset(id, AssetKind::Material, mat.material_path.view());
            }
        }
        if (const EnvironmentState *env = world.try_get_resource<EnvironmentState>()) {
            if (env->texture_path.size() != 0) {
                const AssetId id = AssetId::from_logical_path(env->texture_path.view());
                if (!registry->find(id)) {
                    registry->register_loose_asset(id, AssetKind::Texture,
                                                   env->texture_path.view());
                }
            }
        }
    }

    rebuild_world_transforms(world);

    resolve_primitive_meshes(world, assets, get_ambient_ctx().alloc);

    resolve_render_assets(world, assets);
    resolve_environment(world, assets);

    FR_LOG("[SceneIO] Loaded scene: {}", input_path);
    return true;
}

} // namespace fr::scene
