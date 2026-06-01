/**
 * @file components.hpp
 * @author Tfoedy
 * @brief Core ECS components, or in our case, parts, for the rendering system.
 */
#pragma once

#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/asset_manager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace fr {

/**
 * @brief Defines the shading model applied during Lighting Pass.
 */
enum class ShadingModel : U32 {
    Unlit = 0,    ///< No lighting applied (for example UI, skybox).
    Standard = 1, ///< Classic Blinn-PPhong model.
    PBR = 2       ///< Physically Based Cook-Torrance model.
};

/**
 * @brief Space properties of an entity.
 */
struct TransformPart {
    using vec3_t = glm::vec3;
    using quat_t = glm::quat;

    vec3_t position;
    quat_t rotation;
    vec3_t scale;

    TransformPart() noexcept
        : position(0.0f, 0.0f, 0.0f),
          rotation(1.0f, 0.0f, 0.0f, 0.0f),
          scale(1.0f, 1.0f, 1.0f) {
    }
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
 * @brief Defines the surface visuals and light interactions of an entity.
 */
struct MaterialPart {
    ShadingModel shading_model;

    TextureAssetHandle albedo_map;
    TextureAssetHandle normal_map;
    /// Specular for Standard, metallic/Roughness for PBR
    TextureAssetHandle extra_map;

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

    PointLightPart() noexcept = default;
};

/// @brief Directional light source like the sun
struct DirectionalLightPart {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    F32 intensity{5.0f};
    DirectionalLightPart() noexcept = default;
};

} // namespace fr
