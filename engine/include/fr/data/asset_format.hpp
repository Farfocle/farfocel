/**
 * @file asset_format.hpp
 * @brief Binary on-disk They are written by asscooker and read by AssetManager. * @brief Binary
 * on-disk layout definitions for cooked Farfocel assets.
 *
 * - Runtime asset:
 *   Engine-side asset records such as MeshAsset and TextureAsset.
 *
 * - GPU resource:
 *   RenderDevice handles such as BufferHandle and TextureHandle.
 *
 * The structures in this file belong only to the cooked disk format layer.
 */

#pragma once

#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Texture pixel format stored in a cooked `.ftex` file.
 *
 * @details
 * This enum describes the semantic format of the image payload stored on disk.
 * The runtime AssetManager maps these values to renderer TextureFormat values.
 *
 * Current runtime support:
 *
 * - RGBA8_UNORM
 * - RGBA8_SRGB
 * - RGBA32F_HDR
 *
 * BC5_UNORM is reserved for future normal-map compression support and is not uploaded
 * by the current runtime path yet.
 */
enum class AssetTextureFormat : U32 {
    /// @brief 8-bit unsigned normalized RGBA data, used for non-color data textures.
    RGBA8_UNORM = 0,

    /// @brief 8-bit sRGB RGBA data, used for albedo/color textures.
    RGBA8_SRGB = 1,

    /// @brief Reserved for future two-channel normal map compression.
    BC5_UNORM = 2,

    /// @brief 32-bit floating-point RGBA HDR data.
    RGBA32F_HDR = 3
};

/**
 * @brief Cooked asset file structures are tightly packed.
 *
 * @details
 * These structures are written directly to disk and read back as binary data.
 * Do not add virtual functions, pointers, non-trivial constructors, or platform-dependent
 * runtime-only fields here.
 */
#pragma pack(push, 1)

/**
 * @brief Common header shared by all cooked asset files.
 *
 * @details
 * The verify field identifies the file type.
 *
 * Expected values:
 *
 * - `.fmesh`: "FMSH"
 * - `.ftex`:  "FTEX"
 *
 * The version field is used by the runtime to reject unsupported asset versions.
 */
struct AssetBaseHeader {
    /// @brief Four-byte file type identifier.
    char verify[4];

    /// @brief Cooked file format version.
    U32 version;
};

/**
 * @brief Header stored at the beginning of a `.ftex` file.
 *
 * @details
 * File layout:
 *
 * ```text
 * TextureHeader
 * image byte payload
 * ```
 *
 * The current cooker stores only the base image payload. Runtime mipmaps are generated
 * by the renderer backend when mip_levels is greater than one.
 */
struct TextureHeader {
    /// @brief Common cooked asset header.
    AssetBaseHeader base;

    /// @brief Texture width in pixels.
    U32 width;

    /// @brief Texture height in pixels.
    U32 height;

    /// @brief Pixel format of the stored image payload.
    AssetTextureFormat format;

    /// @brief Size of the following image payload in bytes.
    U32 image_data_size;

    /// @brief Requested number of mip levels.
    U32 mip_levels;
};

/**
 * @brief Header stored at the beginning of a `.fmesh` file.
 *
 * @details
 * File layout:
 *
 * ```text
 * MeshHeader
 * CookedSubMesh[submesh_count]
 * CookedVertex[vertex_count]
 * U32[index_count]
 * char[string_block_size]
 * ```
 *
 * The vertex and index buffers are stored in CPU-friendly form and uploaded directly
 * to the GPU by AssetManager.
 */
struct MeshHeader {
    /// @brief Common cooked asset header.
    AssetBaseHeader base;

    /// @brief Number of CookedVertex entries stored in the file.
    U32 vertex_count;

    /// @brief Number of U32 indices stored in the file.
    U32 index_count;

    /// @brief Number of CookedSubMesh records stored in the file.
    U32 submesh_count;

    /// @brief Size of the cooked vertex payload in bytes.
    U32 vertex_data_size;

    /// @brief Size of the index payload in bytes.
    U32 index_data_size;

    /// @brief Size of the submesh payload in bytes.
    U32 submesh_data_size;

    /// @brief Size of the optional null-terminated string block in bytes.
    U32 string_block_size;

    /// @brief Mesh-space/world-baked minimum AABB extents.
    F32 aabb_min[3];

    /// @brief Mesh-space/world-baked maximum AABB extents.
    F32 aabb_max[3];
};

/**
 * @brief Submesh record stored inside a `.fmesh` file.
 *
 * @details
 * Each submesh describes one indexed draw range and optional material texture references.
 *
 * Texture paths are stored as offsets into the file string block. The value `0xFFFFFFFF`
 * means that the texture path is missing.
 *
 * The renderer uses:
 *
 * ```cpp
 * glDrawElementsBaseVertex(index_count, index_offset, vertex_offset)
 * ```
 *
 * so indices are local to the submesh vertex range and vertex_offset is applied at draw time.
 */
struct CookedSubMesh {
    /// @brief Number of indices to draw.
    U32 index_count;

    /// @brief First index in the mesh index buffer.
    U32 index_offset;

    /// @brief Base vertex offset used for indexed drawing.
    U32 vertex_offset;

    /**
     * @brief Render pass type encoded by the cooker.
     *
     * Current mapping:
     *
     * - 0: Opaque
     * - 1: Masked
     * - 2: Transparent
     */
    U32 pass_type;

    /// @brief Submesh local/world transform stored in column-major matrix layout.
    F32 transform[16];

    /// @brief Offset of the albedo texture path inside the string block, or 0xFFFFFFFF.
    U32 albedo_path_offset;

    /// @brief Offset of the normal texture path inside the string block, or 0xFFFFFFFF.
    U32 normal_path_offset;

    /// @brief Offset of the extra/material texture path inside the string block, or 0xFFFFFFFF.
    U32 extra_path_offset;

    /// @brief Submesh local minimum AABB extents.
    F32 aabb_min[3];

    /// @brief Submesh local maximum AABB extents.
    F32 aabb_max[3];
};

/**
 * @brief Vertex layout stored inside `.fmesh` files and uploaded to the GPU.
 *
 * @details
 * This layout must match the OpenGL vertex attribute setup:
 *
 * ```text
 * location 0: position, vec3, offset 0
 * location 1: normal,   vec3, offset 12
 * location 2: uv,       vec2, offset 24
 * location 3: tangent,  vec4, offset 32
 * stride: 48 bytes
 * ```
 *
 * Tangent.w stores the handedness/sign used to reconstruct the bitangent.
 */
struct CookedVertex {
    /// @brief Vertex position.
    F32 position[3];

    /// @brief Vertex normal.
    F32 normal[3];

    /// @brief Primary texture coordinates.
    F32 uv[2];

    /// @brief Tangent vector and handedness sign.
    F32 tangent[4];
};

#pragma pack(pop)

} // namespace fr
