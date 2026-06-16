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

/**
 * @brief Rebuilds runtime mesh handles for entities with PrimitiveMeshPart.
 */
class PrimitiveMeshSystem {
public:
    static void resolve(World &world, AssetManager &assets, Alloc *alloc) noexcept {
        FR_ASSERT(alloc, "allocator must be non-null");

        /*
            Do not iterate with world.query<PrimitiveMeshPart>() here.

            Some ECS part pools contain a sentinel slot and query iteration can assert when the pool
            is ensured but has no live components. Iterating alive Things and using try_get() is
            safe for optional runtime-dev components.
        */
        world.for_each_alive_thing([&](Thing thing) noexcept {
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

            if (!needs_rebuild(*mesh, *primitive)) {
                return;
            }

            if (mesh->mesh_handle.is_valid()) {
                assets.unload_mesh(mesh->mesh_handle);
                mesh->mesh_handle = {};
            }

            mesh->mesh_path.clear();
            mesh->mesh_id = {};
            mesh->resolved_mesh_id = {};

            MeshAssetHandle handle = create_runtime_primitive_mesh(assets, alloc, *primitive);
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

    static void release(World &world, AssetManager &assets) noexcept {
        world.for_each_alive_thing([&](Thing thing) noexcept {
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

private:
    static bool needs_rebuild(const MeshRendererPart &mesh,
                              const PrimitiveMeshPart &primitive) noexcept {
        if (!mesh.mesh_handle.is_valid()) {
            return true;
        }

        return primitive.resolved_kind != primitive.kind ||
               primitive.resolved_size != primitive.size ||
               primitive.resolved_x_segments != primitive.x_segments ||
               primitive.resolved_z_segments != primitive.z_segments ||
               primitive.resolved_pass_type != primitive.pass_type;
    }

    static RenderPass render_pass_from_u32(U32 pass) noexcept {
        if (pass == static_cast<U32>(RenderPass::Masked)) {
            return RenderPass::Masked;
        }

        if (pass == static_cast<U32>(RenderPass::Transparent)) {
            return RenderPass::Transparent;
        }

        return RenderPass::Opaque;
    }

    static MeshAssetHandle
    create_runtime_primitive_mesh(AssetManager &assets, Alloc *alloc,
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
};

} // namespace fr
