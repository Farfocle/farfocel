/**
 * @file mesh.hpp
 * @author Tfoedy
 * @brief Runtime mesh data used by the renderer.
 *
 * @details
 * MeshData represents GPU mesh resources. It is not the same as the cooked
 * `.fmesh` disk format. AssetManager reads cooked mesh files and converts them into
 * MeshData by creating GPU buffers and runtime SubMesh records.
 */
#pragma once

#include <glm/glm.hpp>

#include "fr/core/dynamic_array.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_sort_key.hpp"

namespace fr {

/**
 * @brief Runtime submesh draw range and material bindings.
 *
 * @details
 * A SubMesh references a draw range inside a mesh index buffer and optional GPU texture
 * handles. Missing textures are replaced by renderer fallback textures during the geometry pass.
 */

struct SubMesh {
    U32 index_count{0};
    U32 index_offset{0};
    U32 vertex_offset{0};
    glm::mat4 transform{1.0f};

    RenderPassType pass_type{RenderPassType::Opaque};

    TextureHandle albedo_map{};
    TextureHandle normal_map{};
    TextureHandle extra_map{};

    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
};

/**
 * @brief GPU-ready mesh data.
 *
 * @details
 * MeshData owns no GPU resources directly, but stores handles created and owned by RenderDevice.
 * AssetManager is responsible for destroying these resources when the mesh asset is unloaded.
 */
struct MeshData {
    BufferHandle vbo{};
    BufferHandle ibo{};
    DynamicArray<SubMesh> submeshes;

    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
};

} // namespace fr
