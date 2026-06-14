/**
 * @file raw_mesh.hpp
 * @author Tfoedy
 * @brief Intermediate mesh data used by the asset cooker.
 */

#pragma once

#include "fr/core/dynamic_array.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_id.hpp"

namespace fr::asscooker {

/**
 * @brief Vertex data before .fmesh compilation.
 */
struct RawVertex {
    F32 position[3];
    F32 normal[3];
    F32 uv[2];
    F32 tangent[4];
};

/**
 * @brief Submesh data before .fmesh compilation.
 */
struct RawSubMesh {
    U32 index_count{0};
    U32 index_offset{0};
    U32 vertex_offset{0};

    /// @brief 0: opaque, 1: masked, 2: transparent.
    U32 pass_type{0};

    F32 transform[16]{};

    AssetId material_id{};

    F32 aabb_min[3]{0.0f, 0.0f, 0.0f};
    F32 aabb_max[3]{0.0f, 0.0f, 0.0f};
};

/**
 * @brief Mesh data ready for .fmesh compilation.
 */
struct RawMesh {
    DynamicArray<RawVertex> vertices;
    DynamicArray<U32> indices;
    DynamicArray<RawSubMesh> submeshes;

    F32 aabb_min[3]{0.0f, 0.0f, 0.0f};
    F32 aabb_max[3]{0.0f, 0.0f, 0.0f};

    explicit RawMesh(Alloc *alloc) noexcept
        : vertices(alloc),
          indices(alloc),
          submeshes(alloc) {
    }
};

} // namespace fr::asscooker
