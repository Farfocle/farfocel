/**
 * @file render_asset_system.hpp
 * @author Tfoedy
 * @brief Resolves render asset ids into runtime handles.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_manager.hpp"
#include "fr/core/meta.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr {

/// @brief Derives mesh_id from mesh_path if a persistent path is set.
inline void sync_mesh_id_from_path(MeshRendererPart &mesh) noexcept {
    if (mesh.mesh_path.size() != 0) {
        mesh.mesh_id = AssetId::from_logical_path(mesh.mesh_path.view());
    }
}

/// @brief Derives material_id from material_path if a persistent path is set.
inline void sync_material_id_from_path(MaterialOverridePart &mat) noexcept {
    if (mat.material_path.size() != 0) {
        mat.material_id = AssetId::from_logical_path(mat.material_path.view());
    }
}

/**
 * @brief Resolves mesh handles for all MeshRendererParts in the world. Skips meshes whose resolved
 * handle is already up to date.
 */
inline void resolve_render_meshes(World &world, AssetManager &assets) noexcept {
    for (auto [thing, mesh] : world.query<MeshRendererPart>()) {
        (void)thing;
        sync_mesh_id_from_path(mesh);

        if (mesh.mesh_id == mesh.resolved_mesh_id && mesh.is_mesh_resolved()) {
            continue;
        }

        if (mesh.is_mesh_resolved()) {
            assets.unload_mesh(mesh.mesh_handle);
            mesh.mesh_handle = {};
            mesh.resolved_mesh_id = {};
        }

        if (!mesh.mesh_id.is_valid()) {
            continue;
        }

        MeshAssetHandle loaded = assets.load_mesh(mesh.mesh_id);
        if (!loaded.is_valid()) {
            FR_LOG_ERR("[render_assets] Failed to resolve mesh. path='{}', id={}",
                       mesh.mesh_path.view(), mesh.mesh_id.value);
            continue;
        }

        mesh.mesh_handle = loaded;
        mesh.resolved_mesh_id = mesh.mesh_id;
    }
}

/**
 * @brief Resolves material handles for all MaterialOverrideParts in the world. Skips materials
 * whose resolved handle is already up to date.
 */
inline void resolve_render_materials(World &world, AssetManager &assets) noexcept {
    for (auto [thing, mat] : world.query<MaterialOverridePart>()) {
        (void)thing;
        sync_material_id_from_path(mat);

        if (mat.material_id == mat.resolved_material_id && mat.is_override_resolved()) {
            continue;
        }

        if (mat.is_override_resolved()) {
            assets.unload_material(mat.material_handle);
            mat.material_handle = {};
            mat.resolved_material_id = {};
        }

        if (!mat.material_id.is_valid()) {
            continue;
        }

        MaterialAssetHandle loaded = assets.load_material(mat.material_id);
        if (!loaded.is_valid()) {
            FR_LOG_ERR("[render_assets] Failed to resolve material. path='{}', id={}",
                       mat.material_path.view(), mat.material_id.value);
            continue;
        }

        mat.material_handle = loaded;
        mat.resolved_material_id = mat.material_id;
    }
}

/**
 * @brief Resolves all render asset handles (meshes + materials) in the world. Call every frame
 * before render extraction.
 */
inline void resolve_render_assets(World &world, AssetManager &assets) noexcept {
    resolve_render_meshes(world, assets);
    resolve_render_materials(world, assets);
}

/// @brief Unloads all mesh handles held by MeshRendererParts in the world.
inline void release_render_meshes(World &world, AssetManager &assets) noexcept {
    for (auto [thing, mesh] : world.query<MeshRendererPart>()) {
        (void)thing;
        if (!mesh.is_mesh_resolved()) {
            continue;
        }

        assets.unload_mesh(mesh.mesh_handle);
        mesh.mesh_handle = {};
        mesh.resolved_mesh_id = {};
    }
}

/// @brief Unloads all material handles held by MaterialOverrideParts in the world.
inline void release_render_materials(World &world, AssetManager &assets) noexcept {
    for (auto [thing, mat] : world.query<MaterialOverridePart>()) {
        (void)thing;
        if (!mat.is_override_resolved()) {
            continue;
        }

        assets.unload_material(mat.material_handle);
        mat.material_handle = {};
        mat.resolved_material_id = {};
    }
}

/// @brief Unloads all render asset handles (meshes + materials) in the world.
/// Call before clearing the scene or shutting down.
inline void release_render_assets(World &world, AssetManager &assets) noexcept {
    release_render_meshes(world, assets);
    release_render_materials(world, assets);
}

/// @brief Unloads all render asset handles owned by a single thing.
inline void release_thing_render_assets(World &world, AssetManager &assets, Thing thing) noexcept {
    if (MeshRendererPart *mesh = world.try_get<MeshRendererPart>(thing)) {
        if (mesh->is_mesh_resolved()) {
            assets.unload_mesh(mesh->mesh_handle);
            mesh->mesh_handle = {};
            mesh->resolved_mesh_id = {};
        }
    }

    if (MaterialOverridePart *mat = world.try_get<MaterialOverridePart>(thing)) {
        if (mat->is_override_resolved()) {
            assets.unload_material(mat->material_handle);
            mat->material_handle = {};
            mat->resolved_material_id = {};
        }
    }
}

/**
 * @brief Unloads the render asset handle for a specific part type on one thing. No-op if the part
 * type does not own render assets.
 */
inline void release_part_render_assets(World &world, AssetManager &assets, Thing thing,
                                       TypeIdx part_type) noexcept {
    if (part_type == TypeIdx::from_type<MeshRendererPart>()) {
        if (MeshRendererPart *mesh = world.try_get<MeshRendererPart>(thing)) {
            if (mesh->is_mesh_resolved()) {
                assets.unload_mesh(mesh->mesh_handle);
                mesh->mesh_handle = {};
                mesh->resolved_mesh_id = {};
            }
        }

        return;
    }

    if (part_type == TypeIdx::from_type<MaterialOverridePart>()) {
        if (MaterialOverridePart *mat = world.try_get<MaterialOverridePart>(thing)) {
            if (mat->is_override_resolved()) {
                assets.unload_material(mat->material_handle);
                mat->material_handle = {};
                mat->resolved_material_id = {};
            }
        }
    }
}

} // namespace fr
