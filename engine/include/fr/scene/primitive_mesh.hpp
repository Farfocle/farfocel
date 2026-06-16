/**
 * @file primitive_mesh_system.hpp
 * @author Tfoedy
 * @brief Runtime primitive mesh rebuild system.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/macros.hpp"
#include "fr/data/world.hpp"
#include "fr/renderer/primitive_meshes.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr {

/// @brief Converts a serialized U32 pass type back to a RenderPass enum value.
inline RenderPass render_pass_from_u32(U32 pass) noexcept {
    if (pass == static_cast<U32>(RenderPass::Masked)) {
        return RenderPass::Masked;
    }

    if (pass == static_cast<U32>(RenderPass::Transparent)) {
        return RenderPass::Transparent;
    }

    return RenderPass::Opaque;
}

/**
 * @brief Returns true if the primitive's resolved state differs from its current desc, meaning the
 * runtime mesh handle needs to be rebuilt.
 */
inline bool primitive_mesh_needs_rebuild(const MeshRendererPart &mesh,
                                         const PrimitiveMeshPart &primitive) noexcept {
    if (!mesh.mesh_handle.is_valid()) {
        return true;
    }

    return primitive.resolved_kind != primitive.kind || primitive.resolved_size != primitive.size ||
           primitive.resolved_x_segments != primitive.x_segments ||
           primitive.resolved_z_segments != primitive.z_segments ||
           primitive.resolved_pass_type != primitive.pass_type;
}

/// @brief Creates a GPU mesh for the given primitive desc and returns the handle.
inline MeshAssetHandle create_primitive_mesh(AssetManager &assets, Alloc *alloc,
                                             const PrimitiveMeshPart &primitive) noexcept {
    const PrimitiveMeshKind kind = static_cast<PrimitiveMeshKind>(primitive.kind);

    primitive_mesh::PrimitiveMeshCreateDesc desc{};
    desc.size = primitive.size;
    desc.pass_type = render_pass_from_u32(primitive.pass_type);

    if (kind == PrimitiveMeshKind::Plane) {
        return primitive_mesh::create_plane(assets, desc);
    }

    if (kind == PrimitiveMeshKind::Grid) {
        primitive_mesh::GridMeshCreateDesc grid_desc{};
        grid_desc.size = primitive.size;
        grid_desc.x_segments = primitive.x_segments > 0 ? primitive.x_segments : 1;
        grid_desc.z_segments = primitive.z_segments > 0 ? primitive.z_segments : 1;
        grid_desc.pass_type = render_pass_from_u32(primitive.pass_type);
        return primitive_mesh::create_grid(assets, alloc, grid_desc);
    }

    return primitive_mesh::create_cube(assets, desc);
}

/**
 * @brief Rebuilds GPU mesh handles for all things with PrimitiveMeshPart whose desc changed.
 * @note Ensures a MeshRendererPart exists on the thing and keeps its handle in sync.
 */
inline void resolve_primitive_meshes(World &world, AssetManager &assets, Alloc *alloc) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    world.each_alive_thing([&](Thing thing) noexcept {
        if (thing.is_nil()) {
            return;
        }

        PrimitiveMeshPart *primitive = world.try_get<PrimitiveMeshPart>(thing);
        if (!primitive) {
            return;
        }

        MeshRendererPart *mesh = world.try_get<MeshRendererPart>(thing);
        if (!mesh) {
            mesh = &world.emplace_now<MeshRendererPart>(thing);
        }

        mesh->visible = true;
        mesh->casts_shadow = primitive->casts_shadow;

        if (!primitive_mesh_needs_rebuild(*mesh, *primitive)) {
            return;
        }

        if (mesh->mesh_handle.is_valid()) {
            assets.unload_mesh(mesh->mesh_handle);
            mesh->mesh_handle = {};
        }
        mesh->mesh_path.clear();
        mesh->mesh_id = {};
        mesh->resolved_mesh_id = {};

        MeshAssetHandle handle = create_primitive_mesh(assets, alloc, *primitive);
        if (!handle.is_valid()) {
            return;
        }

        mesh->mesh_handle = handle;

        primitive->resolved_kind = primitive->kind;
        primitive->resolved_size = primitive->size;
        primitive->resolved_x_segments = primitive->x_segments;
        primitive->resolved_z_segments = primitive->z_segments;
        primitive->resolved_pass_type = primitive->pass_type;
    });
}

/// @brief Unloads GPU mesh handles for all things with PrimitiveMeshPart and resets resolve state.
inline void release_primitive_meshes(World &world, AssetManager &assets) noexcept {
    world.each_alive_thing([&](Thing thing) noexcept {
        if (thing.is_nil()) {
            return;
        }

        PrimitiveMeshPart *primitive = world.try_get<PrimitiveMeshPart>(thing);
        if (!primitive) {
            return;
        }

        MeshRendererPart *mesh = world.try_get<MeshRendererPart>(thing);
        if (!mesh || !mesh->mesh_handle.is_valid()) {
            return;
        }

        assets.unload_mesh(mesh->mesh_handle);
        mesh->mesh_handle = {};
        mesh->resolved_mesh_id = {};

        primitive->resolved_kind = static_cast<U32>(-1);
        primitive->resolved_size = -1.0f;
        primitive->resolved_x_segments = 0;
        primitive->resolved_z_segments = 0;
        primitive->resolved_pass_type = static_cast<U32>(-1);
    });
}

} // namespace fr
