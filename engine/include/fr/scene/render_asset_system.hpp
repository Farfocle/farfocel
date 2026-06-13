/**
 * @file render_asset_system.hpp
 * @author Tfoedy
 * @brief Resolves render asset ids into runtime handles.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/data/world.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr {

/**
 * @brief Resolves render asset references stored in ECS parts.
 *
 * @details
 * This runs before render extraction. Extraction reads resolved handles and does not load assets.
 */
class RenderAssetSystem {
public:
    /**
     * @brief Resolves all render assets required by the world.
     */
    static void resolve(World &world, AssetManager &assets) noexcept {
        resolve_meshes(world, assets);
        resolve_material_overrides(world, assets);
    }

    /**
     * @brief Releases resolved render assets owned by world parts.
     */
    static void release(World &world, AssetManager &assets) noexcept {
        release_meshes(world, assets);
        release_material_overrides(world, assets);
    }

private:
    static void resolve_meshes(World &world, AssetManager &assets) noexcept {
        for (auto [thing, mesh] : world.query<MeshRendererPart>()) {
            (void)thing;

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
                continue;
            }

            mesh.mesh_handle = loaded;
            mesh.resolved_mesh_id = mesh.mesh_id;
        }
    }

    static void resolve_material_overrides(World &world, AssetManager &assets) noexcept {
        for (auto [thing, material] : world.query<MaterialOverridePart>()) {
            (void)thing;

            if (material.material_id == material.resolved_material_id &&
                material.is_override_resolved()) {
                continue;
            }

            if (material.is_override_resolved()) {
                assets.unload_material(material.material_handle);
                material.material_handle = {};
                material.resolved_material_id = {};
            }

            if (!material.material_id.is_valid()) {
                continue;
            }

            MaterialAssetHandle loaded = assets.load_material(material.material_id);
            if (!loaded.is_valid()) {
                continue;
            }

            material.material_handle = loaded;
            material.resolved_material_id = material.material_id;
        }
    }

    static void release_meshes(World &world, AssetManager &assets) noexcept {
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

    static void release_material_overrides(World &world, AssetManager &assets) noexcept {
        for (auto [thing, material] : world.query<MaterialOverridePart>()) {
            (void)thing;

            if (!material.is_override_resolved()) {
                continue;
            }

            assets.unload_material(material.material_handle);

            material.material_handle = {};
            material.resolved_material_id = {};
        }
    }
};

} // namespace fr
