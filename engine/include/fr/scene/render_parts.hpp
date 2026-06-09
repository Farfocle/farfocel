/**
 * @file render_parts.hpp
 * @author Tfoedy
 * @brief Core ECS parts, or in our case, parts, for the rendering system.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "fr/core/typedefs.hpp"
#include "fr/data/asset_manager.hpp"

namespace fr {

/**
 * @brief Defines the shading model applied during Lighting Pass.
 */
enum class ShadingModel : U32 {
    Unlit = 0,    ///< No lighting applied (for example UI, skybox).
    Standard = 1, ///< Classic Blinn-PPhong model.
    PBR = 2       ///< Physically Based Cook-Torrance model.
};

enum class MaterialBlendMode : U8 {
    Opaque = 0,
    Masked = 1,
    Alpha = 2,
};

/**
 * @brief Lens properties of a camera.
 */
struct CameraPart {
    F32 fov{70.0f};
    F32 near_plane{0.1f};
    F32 far_plane{1000.0f};
    bool is_main{true};

    CameraPart() noexcept = default;
};

struct FPSControllerPart {
    F32 pitch{0.0f};
    F32 yaw{-90.0f};
    F32 move_speed{15.0f};
    F32 mouse_sensitivity{0.1f};

    FPSControllerPart() noexcept = default;
};

/**
 * @brief Represents the 3D shape attached to an entity.
 */
struct MeshPart {
    MeshAssetHandle handle;

    MeshPart() noexcept
        : handle() {
    }

    explicit MeshPart(const MeshAssetHandle &h) noexcept
        : handle(h) {
    }
};

/**
 * @brief Defines surface shading parameters used by the renderer.
 *
 * @details
 * The optional extra texture uses a shared material-data layout:
 *
 * - R: metallic for PBR, reserved/specular parameter for Standard
 * - G: roughness
 * - B: ambient occlusion
 * - A: unused in texture input; shading model is written separately to the G-Buffer
 *
 * If a mesh submesh already provides textures from the cooked asset, those textures take
 * prioority. MaterialPart textures are used as a fallbacks.
 */
struct MaterialPart {
    ShadingModel shading_model;

    TextureAssetHandle albedo_map;
    TextureAssetHandle normal_map;
    TextureAssetHandle extra_map;

    MaterialBlendMode blend_mode{MaterialBlendMode::Opaque};
    F32 alpha{1.0f};

    MaterialPart() noexcept
        : shading_model(ShadingModel::PBR),
          albedo_map(),
          normal_map(),
          extra_map() {
    }
};

/// @brief Point light source component
struct PointLightPart {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    F32 intensity{1.0f};
    F32 radius{10.0f};

    bool casts_shadow{false};
    F32 shadow_strength{1.0f};
    F32 shadow_bias{0.005f};

    PointLightPart() noexcept = default;
};

/// @brief Spot light source component.
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

/// @brief Directional light source like the sun
struct DirectionalLightPart {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    F32 intensity{5.0f};
    DirectionalLightPart() noexcept = default;
};

} // namespace fr
