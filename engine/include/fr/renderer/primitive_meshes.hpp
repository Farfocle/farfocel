/**
 * @file primitive_meshes.hpp
 * @author Tfoedy
 * @brief Helpers for creating built-in runtime primitive meshes.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_mesh.hpp"

namespace fr::primitive_mesh {

struct PrimitiveMeshCreateDesc {
    F32 size{1.0f};

    AssetId material_id{};
    MaterialAssetHandle material{};

    RenderPass pass_type{RenderPass::Opaque};

    bool dynamic{false};
};

struct GridMeshCreateDesc {
    U32 x_segments{16};
    U32 z_segments{16};

    F32 size{1.0f};

    AssetId material_id{};
    MaterialAssetHandle material{};

    RenderPass pass_type{RenderPass::Opaque};

    bool dynamic{false};
};

namespace detail {

[[nodiscard]] inline F32 sanitize_size(F32 size) noexcept {
    return size > 0.001f ? size : 0.001f;
}

[[nodiscard]] inline RenderVertex make_vertex(F32 px, F32 py, F32 pz, F32 nx, F32 ny, F32 nz, F32 u,
                                              F32 v, F32 tx, F32 ty, F32 tz, F32 tw) noexcept {
    RenderVertex vertex{};

    vertex.position[0] = px;
    vertex.position[1] = py;
    vertex.position[2] = pz;

    vertex.normal[0] = nx;
    vertex.normal[1] = ny;
    vertex.normal[2] = nz;

    vertex.uv[0] = u;
    vertex.uv[1] = v;

    vertex.tangent[0] = tx;
    vertex.tangent[1] = ty;
    vertex.tangent[2] = tz;
    vertex.tangent[3] = tw;

    return vertex;
}

[[nodiscard]] inline MeshAssetHandle
create_mesh_with_optional_material(AssetManager &assets, RuntimeMeshDesc &mesh_desc,
                                   MaterialAssetHandle material) noexcept {
    if (!material.is_valid()) {
        return assets.create_runtime_mesh(mesh_desc);
    }

    return assets.create_runtime_mesh(mesh_desc, Slice<const MaterialAssetHandle>(&material, 1));
}

} // namespace detail

/**
 * @brief Creates a cube centered at origin.
 */
[[nodiscard]] inline MeshAssetHandle
create_cube(AssetManager &assets, const PrimitiveMeshCreateDesc &create_desc) noexcept {
    const F32 size = detail::sanitize_size(create_desc.size);
    const F32 h = size * 0.5f;

    const RenderVertex vertices[] = {
        detail::make_vertex(h, -h, -h, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f),
        detail::make_vertex(h, -h, h, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f),
        detail::make_vertex(h, h, h, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 1.0f),
        detail::make_vertex(h, h, -h, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 1.0f),

        detail::make_vertex(-h, -h, h, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f),
        detail::make_vertex(-h, -h, -h, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f),
        detail::make_vertex(-h, h, -h, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
        detail::make_vertex(-h, h, h, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f),

        detail::make_vertex(-h, h, -h, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, h, -h, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, h, h, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(-h, h, h, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f),

        detail::make_vertex(-h, -h, h, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, -h, h, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, -h, -h, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(-h, -h, -h, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f),

        detail::make_vertex(h, -h, h, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(-h, -h, h, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(-h, h, h, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, h, h, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f),

        detail::make_vertex(-h, -h, -h, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, -h, -h, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, h, -h, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(-h, h, -h, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
    };

    const U32 indices[] = {
        0,  2,  1,  0,  3,  2,  4,  6,  5,  4,  7,  6,  8,  10, 9,  8,  11, 10,
        12, 14, 13, 12, 15, 14, 16, 18, 17, 16, 19, 18, 20, 22, 21, 20, 23, 22,
    };

    RuntimeSubMeshDesc submesh{};
    submesh.index_count = static_cast<U32>(sizeof(indices) / sizeof(indices[0]));
    submesh.index_offset = 0;
    submesh.vertex_offset = 0;
    submesh.pass_type = create_desc.pass_type;
    submesh.material_id = create_desc.material_id;
    submesh.aabb_min = glm::vec3(-h, -h, -h);
    submesh.aabb_max = glm::vec3(h, h, h);

    RuntimeMeshDesc mesh_desc{};
    mesh_desc.vertices =
        Slice<const RenderVertex>(vertices, sizeof(vertices) / sizeof(vertices[0]));
    mesh_desc.indices = Slice<const U32>(indices, sizeof(indices) / sizeof(indices[0]));
    mesh_desc.submeshes = Slice<const RuntimeSubMeshDesc>(&submesh, 1);
    mesh_desc.aabb_min = submesh.aabb_min;
    mesh_desc.aabb_max = submesh.aabb_max;
    mesh_desc.dynamic = create_desc.dynamic;

    return detail::create_mesh_with_optional_material(assets, mesh_desc, create_desc.material);
}

/**
 * @brief Creates a cube centered at origin.
 */
[[nodiscard]] inline MeshAssetHandle create_cube(AssetManager &assets, F32 size = 1.0f) noexcept {
    PrimitiveMeshCreateDesc desc{};
    desc.size = size;
    return create_cube(assets, desc);
}

/**
 * @brief Creates an XZ plane centered at origin.
 */
[[nodiscard]] inline MeshAssetHandle
create_plane(AssetManager &assets, const PrimitiveMeshCreateDesc &create_desc) noexcept {
    const F32 size = detail::sanitize_size(create_desc.size);
    const F32 h = size * 0.5f;

    const RenderVertex vertices[] = {
        detail::make_vertex(-h, 0.0f, -h, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, 0.0f, -h, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(h, 0.0f, h, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        detail::make_vertex(-h, 0.0f, h, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
    };

    const U32 indices[] = {0, 2, 1, 0, 3, 2};

    RuntimeSubMeshDesc submesh{};
    submesh.index_count = static_cast<U32>(sizeof(indices) / sizeof(indices[0]));
    submesh.index_offset = 0;
    submesh.vertex_offset = 0;
    submesh.pass_type = create_desc.pass_type;
    submesh.material_id = create_desc.material_id;
    submesh.aabb_min = glm::vec3(-h, 0.0f, -h);
    submesh.aabb_max = glm::vec3(h, 0.0f, h);

    RuntimeMeshDesc mesh_desc{};
    mesh_desc.vertices =
        Slice<const RenderVertex>(vertices, sizeof(vertices) / sizeof(vertices[0]));
    mesh_desc.indices = Slice<const U32>(indices, sizeof(indices) / sizeof(indices[0]));
    mesh_desc.submeshes = Slice<const RuntimeSubMeshDesc>(&submesh, 1);
    mesh_desc.aabb_min = submesh.aabb_min;
    mesh_desc.aabb_max = submesh.aabb_max;
    mesh_desc.dynamic = create_desc.dynamic;

    return detail::create_mesh_with_optional_material(assets, mesh_desc, create_desc.material);
}

/**
 * @brief Creates an XZ plane centered at origin.
 */
[[nodiscard]] inline MeshAssetHandle create_plane(AssetManager &assets, F32 size = 1.0f) noexcept {
    PrimitiveMeshCreateDesc desc{};
    desc.size = size;
    return create_plane(assets, desc);
}

/**
 * @brief Creates an XZ grid mesh centered at origin.
 */
[[nodiscard]] inline MeshAssetHandle create_grid(AssetManager &assets, Alloc *alloc,
                                                 const GridMeshCreateDesc &create_desc) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (create_desc.x_segments == 0 || create_desc.z_segments == 0) {
        return {};
    }

    const F32 size = detail::sanitize_size(create_desc.size);

    const U32 vertex_count_x = create_desc.x_segments + 1;
    const U32 vertex_count_z = create_desc.z_segments + 1;

    const USize vertex_count =
        static_cast<USize>(vertex_count_x) * static_cast<USize>(vertex_count_z);

    const USize index_count = static_cast<USize>(create_desc.x_segments) *
                              static_cast<USize>(create_desc.z_segments) * 6u;

    if (vertex_count > static_cast<USize>(0xFFFFFFFFu) ||
        index_count > static_cast<USize>(0xFFFFFFFFu)) {
        return {};
    }

    DynamicArray<RenderVertex> vertices(alloc);
    DynamicArray<U32> indices(alloc);

    vertices.reserve(vertex_count);
    indices.reserve(index_count);

    const F32 half_size = size * 0.5f;
    const F32 dx = size / static_cast<F32>(create_desc.x_segments);
    const F32 dz = size / static_cast<F32>(create_desc.z_segments);

    for (U32 z = 0; z < vertex_count_z; ++z) {
        for (U32 x = 0; x < vertex_count_x; ++x) {
            const F32 px = -half_size + static_cast<F32>(x) * dx;
            const F32 pz = -half_size + static_cast<F32>(z) * dz;

            const F32 u = static_cast<F32>(x) / static_cast<F32>(create_desc.x_segments);
            const F32 v = static_cast<F32>(z) / static_cast<F32>(create_desc.z_segments);

            vertices.push_back(
                detail::make_vertex(px, 0.0f, pz, 0.0f, 1.0f, 0.0f, u, v, 1.0f, 0.0f, 0.0f, 1.0f));
        }
    }

    for (U32 z = 0; z < create_desc.z_segments; ++z) {
        for (U32 x = 0; x < create_desc.x_segments; ++x) {
            const U32 i0 = z * vertex_count_x + x;
            const U32 i1 = i0 + 1;
            const U32 i2 = i0 + vertex_count_x + 1;
            const U32 i3 = i0 + vertex_count_x;
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i0);
            indices.push_back(i3);
            indices.push_back(i2);
        }
    }

    RuntimeSubMeshDesc submesh{};
    submesh.index_count = static_cast<U32>(indices.size());
    submesh.index_offset = 0;
    submesh.vertex_offset = 0;
    submesh.pass_type = create_desc.pass_type;
    submesh.material_id = create_desc.material_id;
    submesh.aabb_min = glm::vec3(-half_size, 0.0f, -half_size);
    submesh.aabb_max = glm::vec3(half_size, 0.0f, half_size);

    RuntimeMeshDesc mesh_desc{};
    mesh_desc.vertices = Slice<const RenderVertex>(vertices.data(), vertices.size());
    mesh_desc.indices = Slice<const U32>(indices.data(), indices.size());
    mesh_desc.submeshes = Slice<const RuntimeSubMeshDesc>(&submesh, 1);
    mesh_desc.aabb_min = submesh.aabb_min;
    mesh_desc.aabb_max = submesh.aabb_max;
    mesh_desc.dynamic = create_desc.dynamic;

    return detail::create_mesh_with_optional_material(assets, mesh_desc, create_desc.material);
}

/**
 * @brief Creates an XZ grid mesh centered at origin.
 */
[[nodiscard]] inline MeshAssetHandle create_grid(AssetManager &assets, Alloc *alloc, U32 x_segments,
                                                 U32 z_segments, F32 size = 1.0f) noexcept {
    GridMeshCreateDesc desc{};
    desc.x_segments = x_segments;
    desc.z_segments = z_segments;
    desc.size = size;

    return create_grid(assets, alloc, desc);
}

} // namespace fr::primitive_mesh
