/**
 * @file render_parts.hpp
 * @author Tfoedy
 * @brief ECS parts used by scene rendering.
 */

#pragma once

#include <glm/glm.hpp>

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_manager.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

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

    FR_SHAPE({
        FR_PROP(fov);
        FR_PROP(near_plane);
        FR_PROP(far_plane);
        FR_PROP(is_main);
    })
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

    FR_SHAPE({
        FR_PROP(pitch);
        FR_PROP(yaw);
        FR_PROP(move_speed);
        FR_PROP(mouse_sensitivity);
    })
};

/**
 * @brief Renderable mesh reference.
 *
 * @details
 * mesh_path is persistent scene data and should point to a cooked logical .fmesh path.
 * mesh_id is derived from mesh_path or can be assigned directly by runtime code.
 * mesh_handle and resolved_mesh_id are runtime caches owned by RenderAssetSystem.
 */
struct MeshRendererPart {
    String mesh_path{};
    AssetId mesh_id{};

    AssetId resolved_mesh_id{};
    MeshAssetHandle mesh_handle{};

    bool visible{true};
    bool casts_shadow{true};

    MeshRendererPart() noexcept = default;

    explicit MeshRendererPart(AssetId id) noexcept
        : mesh_id(id) {
    }

    explicit MeshRendererPart(StringView path)
        : mesh_path(String::from_view(path)),
          mesh_id(AssetId::from_logical_path(path)) {
    }

    MeshRendererPart(AssetId id, MeshAssetHandle loaded_mesh) noexcept
        : mesh_id(id),
          resolved_mesh_id(id),
          mesh_handle(loaded_mesh) {
    }

    [[nodiscard]] bool is_mesh_resolved() const noexcept {
        return mesh_handle.is_valid();
    }

    FR_SHAPE({
        FR_PROP(mesh_path);
        FR_PROP(visible);
        FR_PROP(casts_shadow);
    })
};

/**
 * @brief Optional material override for a mesh renderer.
 *
 * @details
 * material_path is persistent scene data and should point to a cooked logical .fmat path.
 * material_id is derived from material_path or can be assigned directly by runtime code.
 * material_handle and resolved_material_id are runtime caches owned by RenderAssetSystem.
 */
struct MaterialOverridePart {
    String material_path{};
    AssetId material_id{};

    AssetId resolved_material_id{};
    MaterialAssetHandle material_handle{};

    MaterialOverridePart() noexcept = default;

    explicit MaterialOverridePart(AssetId id) noexcept
        : material_id(id) {
    }

    explicit MaterialOverridePart(StringView path)
        : material_path(String::from_view(path)),
          material_id(AssetId::from_logical_path(path)) {
    }

    MaterialOverridePart(AssetId id, MaterialAssetHandle loaded_material) noexcept
        : material_id(id),
          resolved_material_id(id),
          material_handle(loaded_material) {
    }

    [[nodiscard]] bool is_override_resolved() const noexcept {
        return material_handle.is_valid();
    }

    FR_SHAPE({ FR_PROP(material_path); })
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

    FR_SHAPE({
        FR_PROP(color);
        FR_PROP(intensity);
        FR_PROP(radius);
        FR_PROP(casts_shadow);
        FR_PROP(shadow_strength);
        FR_PROP(shadow_bias);
    })
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

    FR_SHAPE({
        FR_PROP(color);
        FR_PROP(intensity);
        FR_PROP(radius);
        FR_PROP(inner_angle_deg);
        FR_PROP(outer_angle_deg);
        FR_PROP(casts_shadow);
        FR_PROP(shadow_strength);
        FR_PROP(shadow_bias);
    })
};

/**
 * @brief Directional light component.
 */
struct DirectionalLightPart {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    F32 intensity{5.0f};

    DirectionalLightPart() noexcept = default;

    FR_SHAPE({
        FR_PROP(color);
        FR_PROP(intensity);
    })
};

} // namespace fr

FR_TYPE(fr::CameraPart);
FR_TYPE(fr::FPSControllerPart);
FR_TYPE(fr::MeshRendererPart);
FR_TYPE(fr::MaterialOverridePart);
FR_TYPE(fr::PointLightPart);
FR_TYPE(fr::SpotLightPart);
FR_TYPE(fr::DirectionalLightPart);
