/**
 * @file render_light_data.hpp
 * @author Tfoedy
 * @brief GPU-side light and shadow data.
 */

#pragma once

#include <glm/glm.hpp>

#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief Maximum number of point lights with shadow data submitted per frame.
constexpr USize MAX_SHADOWED_POINT_LIGHTS = 4;

/// @brief Maximum number of spot lights submitted per frame.
constexpr USize MAX_SPOT_LIGHTS = 64;

/// @brief Maximum number of spot lights with shadow data submitted per frame.
constexpr USize MAX_SHADOWED_SPOT_LIGHTS = 4;

/// @brief Number of cubemap faces rendered for one point shadow.
constexpr USize POINT_SHADOW_FACE_COUNT = 6;

/**
 * @brief GPU-side point light data.
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
 * @brief GPU-side spot light data.
 *
 * position_radius.xyz     = position
 * position_radius.w       = radius
 * direction_intensity.xyz = direction
 * direction_intensity.w   = intensity
 * color_inner_cos.xyz     = color
 * color_inner_cos.w       = inner cone cosine
 * shadow_params.x         = outer cone cosine
 * shadow_params.y         = shadow index
 * shadow_params.z         = shadow strength
 * shadow_params.w         = shadow bias
 */
struct alignas(16) SpotLightData {
    glm::vec4 position_radius;
    glm::vec4 direction_intensity;
    glm::vec4 color_inner_cos;
    glm::vec4 shadow_params;
};

/**
 * @brief GPU-side point shadow data.
 */
struct alignas(16) PointShadowData {
    glm::mat4 view_proj[POINT_SHADOW_FACE_COUNT];
    glm::vec4 position_radius;
};

/**
 * @brief GPU-side spot shadow data.
 */
struct alignas(16) SpotShadowData {
    glm::mat4 view_proj;
    glm::vec4 position_radius;
    glm::vec4 direction_bias;
};

/**
 * @brief GPU-side directional light data.
 *
 * shadow_params:
 * - x: minimum depth bias
 * - y: slope-scaled bias factor
 * - z: per-cascade bias scale
 * - w: shadow strength
 *
 * shadow_filter_params:
 * - x: base PCF radius in atlas texels
 * - y: additional radius per cascade
 * - z: reserved
 * - w: reserved
 */
struct alignas(16) DirectionalLightData {
    glm::vec3 direction;
    F32 intensity;

    glm::vec3 color;
    F32 padding;

    glm::mat4 light_view_proj[4];
    glm::vec4 cascade_splits;
    glm::vec4 shadow_params;
    glm::vec4 shadow_filter_params;
};

} // namespace fr
