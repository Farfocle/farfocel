/**
 * @file render_lights.hpp
 * @author Tfoedy
 * @brief GPU light and shadow payloads.
 */

#pragma once

#include <glm/glm.hpp>

#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief Maximum point lights with shadow maps submitted per frame.
constexpr USize MAX_SHADOWED_POINT_LIGHTS = 4;

/// @brief Maximum spot lights submitted per frame.
constexpr USize MAX_SPOT_LIGHTS = 64;

/// @brief Maximum spot lights with shadow maps submitted per frame.
constexpr USize MAX_SHADOWED_SPOT_LIGHTS = 4;

/// @brief Cubemap face count for one point light shadow.
constexpr USize POINT_SHADOW_FACE_COUNT = 6;

/// @brief Cascades rendered for the directional shadow atlas.
constexpr USize DIRECTIONAL_CASCADE_COUNT = 3;

/**
 * @brief Point light data consumed by lighting shaders.
 */
struct alignas(16) PointLightData {
    glm::vec3 position;
    F32 radius;

    glm::vec3 color;
    F32 intensity;

    S32 shadow_index{-1};
    F32 shadow_strength{1.0f};
    F32 shadow_bias{0.005f};
    F32 padding{0.0f};
};

/**
 * @brief Spot light data consumed by lighting shaders.
 *
 * @details
 * Packed as vec4 fields to match shader storage layout.
 */
struct alignas(16) SpotLightData {
    /// xyz: position, w: radius.
    glm::vec4 position_radius;

    /// xyz: direction, w: intensity.
    glm::vec4 direction_intensity;

    /// xyz: color, w: inner cone cosine.
    glm::vec4 color_inner_cos;

    /// x: outer cone cosine, y: shadow index, z: strength, w: bias.
    glm::vec4 shadow_params;
};

/**
 * @brief View-projection data for one point shadow cubemap.
 */
struct alignas(16) PointShadowData {
    glm::mat4 view_proj[POINT_SHADOW_FACE_COUNT];
    glm::vec4 position_radius;
};

/**
 * @brief View-projection data for one spot shadow.
 */
struct alignas(16) SpotShadowData {
    glm::mat4 view_proj;
    glm::vec4 position_radius;
    glm::vec4 direction_bias;
};

/**
 * @brief Directional light data consumed by lighting and shadow passes.
 *
 * @details
 * The current renderer uses DIRECTIONAL_CASCADE_COUNT cascades for the primary directional
 * shadow atlas.
 */
struct alignas(16) DirectionalLightData {
    glm::vec3 direction;
    F32 intensity;

    glm::vec3 color;
    F32 padding;

    glm::mat4 light_view_proj[DIRECTIONAL_CASCADE_COUNT];

    /// x/y/z: cascade split distances, w: reserved.
    glm::vec4 cascade_splits;

    /// x: min bias, y: slope bias, z: cascade bias scale, w: shadow strength.
    glm::vec4 shadow_params;

    /// x: base PCF radius, y: cascade radius scale, z/w: reserved.
    glm::vec4 shadow_filter_params;
};

} // namespace fr
