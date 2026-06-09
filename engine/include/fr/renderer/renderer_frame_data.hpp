/**
 * @file renderer_frame_data.hpp
 * @author Tfoedy
 * @brief GPU-side per-frame renderer data.
 */

#pragma once

#include "fr/core/typedefs.hpp"

#include <glm/glm.hpp>

namespace fr {

/**
 * @brief Camera and per-frame data uploaded to the GPU.
 *
 * counts_debug:
 * - x: point light count
 * - y: spot light count
 * - z: directional light count
 * - w: debug mode
 *
 * ibl_params:
 * - x: environment ready
 * - y: irradiance ready
 * - z: diffuse strength
 * - w: specular strength
 *
 * ibl_params2:
 * - x: occlusion strength
 * - y: occlusion power
 * - z: sky visibility strength
 * - w: enabled
 *
 * ibl_params3:
 * - x: prefiltered ready
 * - y: BRDF LUT ready
 * - z: max prefiltered LOD
 * - w: reserved
 */
struct alignas(16) CameraData {
    glm::mat4 view_proj;
    glm::mat4 inv_view_proj;

    glm::vec4 cam_pos;
    glm::vec4 cam_forward;

    glm::uvec4 counts_debug;
    glm::uvec4 flags_reserved;

    glm::vec4 lighting_params;
    glm::vec4 ao_params;
    glm::vec4 ao_params2;

    glm::vec4 ibl_params;
    glm::vec4 ibl_params2;
    glm::vec4 ibl_params3;
};

} // namespace fr
