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
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_sort_key.hpp"

namespace fr {

constexpr U32 INVALID_RENDER_SUBMESH_MATERIAL_INDEX = 0xFFFFFFFFu;

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
