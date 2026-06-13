/**
 * @file render_mesh.hpp
 * @author Tfoedy
 * @brief Runtime mesh data used by render extraction.
 */

#pragma once

#include <glm/glm.hpp>

#include "fr/asset/asset_id.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_sort_key.hpp"

namespace fr {

constexpr U32 INVALID_RENDER_SUBMESH_MATERIAL_INDEX = 0xFFFFFFFFu;

/**
 * @brief Runtime vertex layout consumed by renderer mesh pipelines.
 *
 * @details
 * This layout intentionally matches CookedVertex so procedural meshes and cooked meshes use the
 * same GPU vertex format.
 */
struct RenderVertex {
    F32 position[3]{0.0f, 0.0f, 0.0f};
    F32 normal[3]{0.0f, 1.0f, 0.0f};
    F32 uv[2]{0.0f, 0.0f};
    F32 tangent[4]{1.0f, 0.0f, 0.0f, 1.0f};
};

/**
 * @brief Runtime submesh description used when creating meshes from code.
 */
struct RuntimeSubMeshDesc {
    U32 index_count{0};
    U32 index_offset{0};
    U32 vertex_offset{0};

    RenderPass pass_type{RenderPass::Opaque};

    AssetId material_id{};

    glm::mat4 transform{1.0f};

    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
};

/**
 * @brief Runtime mesh creation descriptor.
 */
struct RuntimeMeshDesc {
    Slice<const RenderVertex> vertices{};
    Slice<const U32> indices{};
    Slice<const RuntimeSubMeshDesc> submeshes{};

    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};

    bool dynamic{false};
};

/**
 * @brief Draw range and material reference for one mesh section.
 */
struct RenderSubMesh {
    U32 index_count{0};
    U32 index_offset{0};
    U32 vertex_offset{0};

    /// @brief Local transform baked from the imported node hierarchy.
    glm::mat4 transform{1.0f};

    RenderPass pass_type{RenderPass::Opaque};

    /// @brief Logical material reference stored in the cooked mesh.
    AssetId material_id{};

    /// @brief Index into RenderMeshData::material_deps owned by AssetManager.
    U32 material_index{INVALID_RENDER_SUBMESH_MATERIAL_INDEX};

    /// @brief Local-space bounds before applying transform.
    glm::vec3 aabb_min{0.0f};

    /// @brief Local-space bounds before applying transform.
    glm::vec3 aabb_max{0.0f};
};

/**
 * @brief Mesh asset data ready for renderer submission.
 */
struct RenderMeshData {
    BufferHandle vbo{};
    BufferHandle ibo{};

    DynamicArray<RenderSubMesh> submeshes;

    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};

    RenderMeshData() noexcept = default;

    explicit RenderMeshData(Alloc *alloc) noexcept
        : submeshes(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");
    }
};

} // namespace fr
