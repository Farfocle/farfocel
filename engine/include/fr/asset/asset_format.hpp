/**
 * @file asset_format.hpp
 * @author Tfoedy
 * @brief Binary layouts for cooked Farfocel assets.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief Texture payload format stored in .ftex.
enum class CookedTextureFormat : U32 {
    RGBA8_UNORM = 0,
    RGBA8_SRGB = 1,

    /**
     * @brief Value 2 is unused. Older experimental builds reserved it for BC5 data, but
     * the current runtime does not support that texture path.
     */
    RGBA32F_HDR = 3
};

#pragma pack(push, 1)

/// @brief Common header stored at the beginning of every cooked asset file.
struct CookedAssetHeader {
    char verify[4];
    U32 version;
};

/// @brief Header stored at the beginning of .ftex.
struct CookedTextureHeader {
    CookedAssetHeader base;

    U32 width;
    U32 height;

    CookedTextureFormat format;

    U32 image_data_size;
    U32 mip_levels;
};

/// @brief Header stored at the beginning of .fmesh.
struct CookedMeshHeader {
    CookedAssetHeader base;

    U32 vertex_count;
    U32 index_count;
    U32 submesh_count;

    U32 vertex_data_size;
    U32 index_data_size;
    U32 submesh_data_size;

    /// @brief Reserved. Must be zero for .fmesh version 2.
    U32 reserved0;

    F32 aabb_min[3];
    F32 aabb_max[3];
};

/// @brief Submesh record stored inside .fmesh.
struct CookedSubMesh {
    U32 index_count;
    U32 index_offset;
    U32 vertex_offset;

    /// @brief 0: opaque, 1: masked, 2: transparent.
    U32 pass_type;

    AssetId material_id;

    /// @brief Column-major local transform.
    F32 transform[16];

    F32 aabb_min[3];
    F32 aabb_max[3];
};

/**
 * @brief Vertex layout stored inside .fmesh.
 *
 * @details
 * Matches the renderer vertex input layout:
 * - location 0: position, vec3, offset 0
 * - location 1: normal, vec3, offset 12
 * - location 2: uv, vec2, offset 24
 * - location 3: tangent, vec4, offset 32
 */
struct CookedVertex {
    F32 position[3];
    F32 normal[3];
    F32 uv[2];
    F32 tangent[4];
};

#pragma pack(pop)

FR_STATIC_ASSERT(sizeof(AssetId) == sizeof(U64), "AssetId must remain a 64-bit disk value");
FR_STATIC_ASSERT(sizeof(CookedVertex) == 48,
                 "CookedVertex layout must match renderer input stride");

} // namespace fr
