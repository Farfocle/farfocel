/**
 * @file render_parts.hpp
 * @author Tfoedy
 * @brief ECS parts used by scene rendering.
 */

#pragma once

#include <glm/glm.hpp>

#include "fr/core/typedefs.hpp"
#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_manager.hpp"

namespace fr {

/**
 * @brief Camera lens properties.
 */
struct CameraPart {
    F32 fov{70.0f};
    F32 near_plane{0.1f};
    F32 far_plane{1000.0f};
    bool is_main{true};

    CameraPart() noexcept = default;
};

/**
 * @brief Simple first-person camera controller state.
 */
struct FPSControllerPart {
    F32 pitch{0.0f};
    F32 yaw{-90.0f};
    F32 move_speed{15.0f};
    F32 mouse_sensitivity{0.1f};

    FPSControllerPart() noexcept = default;
};

/**
 * @brief Renderable mesh reference.
 *
 * @details
 * mesh_id is stable scene data. mesh_handle is a runtime cache resolved by
 * RenderAssetSystem.
 */
struct MeshRendererPart {
    AssetId mesh_id{};
    AssetId resolved_mesh_id{};
    MeshAssetHandle mesh_handle{};

    bool visible{true};
    bool casts_shadow{true};

    MeshRendererPart() noexcept = default;

    explicit MeshRendererPart(AssetId id) noexcept
        : mesh_id(id) {
    }

    MeshRendererPart(AssetId id, MeshAssetHandle loaded_mesh) noexcept
        : mesh_id(id),
          resolved_mesh_id(id),
          mesh_handle(loaded_mesh) {
    }

    [[nodiscard]] bool is_mesh_resolved() const noexcept {
        return mesh_handle.is_valid();
    }
};

/**
 * @brief Optional material override for a mesh renderer.
 *
 * @details
 * If resolved, this material replaces submesh materials during extraction.
 */
struct MaterialOverridePart {
    AssetId material_id{};
    AssetId resolved_material_id{};
    MaterialAssetHandle material_handle{};

    MaterialOverridePart() noexcept = default;

    explicit MaterialOverridePart(AssetId id) noexcept
        : material_id(id) {
    }

    MaterialOverridePart(AssetId id, MaterialAssetHandle loaded_material) noexcept
        : material_id(id),
          resolved_material_id(id),
          material_handle(loaded_material) {
    }

    [[nodiscard]] bool is_override_resolved() const noexcept {
        return material_handle.is_valid();
    }
};

/**
 * @brief Point light component.
 */
struct PointLightPart {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    F32 intensity{1.0f};
    F32 radius{10.0f};

    bool casts_shadow{false};
    F32 shadow_strength{1.0f};
    F32 shadow_bias{0.005f};

    PointLightPart() noexcept = default;
};

/**
 * @brief Spot light component.
 */
struct SpotLightPart {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    F32 intensity{0.0f};
    F32 radius{25.0f};

    F32 inner_angle_deg{20.0f};
    F32 outer_angle_deg{35.0f};

    bool casts_shadow{false};
    F32 shadow_strength{1.0f};
    F32 shadow_bias{0.002f};

    SpotLightPart() noexcept = default;
};

/**
 * @brief Directional light component.
 */
struct DirectionalLightPart {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    F32 intensity{5.0f};

    DirectionalLightPart() noexcept = default;
};

} // namespace fr
