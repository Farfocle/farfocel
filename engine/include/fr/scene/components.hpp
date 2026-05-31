/**
 * @file components.hpp
 * @author Tfoedy
 * @brief Core ECS components for the rendering system.
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
struct TransformComponent {
    using vec3_t = glm::vec3;
    using quat_t = glm::quat;

    vec3_t position;
    quat_t rotation;
    vec3_t scale;

    TransformComponent() noexcept
        : position(0.0f, 0.0f, 0.0f),
          rotation(1.0f, 0.0f, 0.0f, 0.0f),
          scale(1.0f, 1.0f, 1.0f) {
    }
};

/**
 * @brief Lens properties of a camera.
 */
struct CameraComponent {
    F32 fov;
    F32 near_plane;
    F32 far_plane;
    bool is_main;

    CameraComponent() noexcept
        : fov(70.0f),
          near_plane(0.1f),
          far_plane(1000.0f),
          is_main(true) {
    }
};

/**
 * @brief Represents the 3D shape attached to an entity.
 */
struct MeshComponent {
    MeshAssetHandle handle;

    MeshComponent() noexcept
        : handle() {
    }

    explicit MeshComponent(const MeshAssetHandle &h) noexcept
        : handle(h) {
    }
};

/**
 * @brief Defines the surface visuals and light interactions of an entity.
 */
struct MaterialComponent {
    ShadingModel shading_model;

    TextureAssetHandle albedo_map;
    TextureAssetHandle normal_map;
    /// Specular for Standard, metallic/Roughness for PBR
    TextureAssetHandle extra_map;

    MaterialComponent() noexcept
        : shading_model(ShadingModel::PBR),
          albedo_map(),
          normal_map(),
          extra_map() {
    }
};

} // namespace fr
