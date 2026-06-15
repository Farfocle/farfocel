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

/**
 * @brief Resolves render asset references stored in ECS parts.
 *
 * @details
 * This runs before render extraction. Extraction reads resolved handles and does not load assets.
 *
 * Persistent scene data stores cooked logical asset paths. Runtime asset ids are derived from those
 * paths, while resolved handles are treated as transient caches.
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

    /**
     * @brief Releases all render asset handles owned by one thing.
     */
    static void release_thing(World &world, AssetManager &assets, Thing thing) noexcept {
        release_mesh_renderer(world, assets, thing);
        release_material_override(world, assets, thing);
    }

    /**
     * @brief Releases render asset handles owned by a specific part type on one thing.
     */
    static void release_part(World &world, AssetManager &assets, Thing thing,
                             TypeIdx part_type) noexcept {
        if (part_type == TypeIdx::from_type<MeshRendererPart>()) {
            release_mesh_renderer(world, assets, thing);
            return;
        }

        if (part_type == TypeIdx::from_type<MaterialOverridePart>()) {
            release_material_override(world, assets, thing);
            return;
        }
    }

private:
    /**
     * @brief Refreshes mesh_id from mesh_path when a persistent path is available.
     */
    static void sync_mesh_id_from_path(MeshRendererPart &mesh) noexcept {
        if (mesh.mesh_path.size() == 0) {
            return;
        }

        mesh.mesh_id = AssetId::from_logical_path(mesh.mesh_path.view());
    }

    /**
     * @brief Refreshes material_id from material_path when a persistent path is available.
     */
    static void sync_material_id_from_path(MaterialOverridePart &material) noexcept {
        if (material.material_path.size() == 0) {
            return;
        }

        material.material_id = AssetId::from_logical_path(material.material_path.view());
    }

    static void resolve_meshes(World &world, AssetManager &assets) noexcept {
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
                FR_LOG_ERR("[RenderAssetSystem] Failed to resolve mesh asset. path='{}', id={}",
                           mesh.mesh_path.view(), mesh.mesh_id.value);
                continue;
            }

            mesh.mesh_handle = loaded;
            mesh.resolved_mesh_id = mesh.mesh_id;
        }
    }

    static void resolve_material_overrides(World &world, AssetManager &assets) noexcept {
        for (auto [thing, material] : world.query<MaterialOverridePart>()) {
            (void)thing;

            sync_material_id_from_path(material);

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
                FR_LOG_ERR("[RenderAssetSystem] Failed to resolve material asset. path='{}', id={}",
                           material.material_path.view(), material.material_id.value);
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

    static void release_mesh_renderer(World &world, AssetManager &assets, Thing thing) noexcept {
        MeshRendererPart *mesh = world.try_get<MeshRendererPart>(thing);
        if (!mesh || !mesh->is_mesh_resolved()) {
            return;
        }

        assets.unload_mesh(mesh->mesh_handle);

        mesh->mesh_handle = {};
        mesh->resolved_mesh_id = {};
    }

    static void release_material_override(World &world, AssetManager &assets,
                                          Thing thing) noexcept {
        MaterialOverridePart *material = world.try_get<MaterialOverridePart>(thing);
        if (!material || !material->is_override_resolved()) {
            return;
        }

        assets.unload_material(material->material_handle);

        material->material_handle = {};
        material->resolved_material_id = {};
    }
};

} // namespace fr
