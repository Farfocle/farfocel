/**
 * @file render_gpu_frame.hpp
 * @author Tfoedy
 * @brief GPU frame data layouts used by renderer SSBOs.
 */

#pragma once

#include <glm/glm.hpp>

namespace fr {

/**
 * @brief Camera and frame constants uploaded once per frame.
 *
 * @details
 * Keep this layout in sync with CameraSSBO blocks in renderer shaders.
 */
struct alignas(16) GpuCameraData {
    glm::mat4 view_proj;
    glm::mat4 inv_view_proj;

    glm::vec4 cam_pos;
    glm::vec4 cam_forward;

    /// x: point lights, y: spot lights, z: directional lights, w: debug mode.
    glm::uvec4 counts_debug;

    /// x: debug flags, y/z/w: reserved.
    glm::uvec4 flags_reserved;

    /// x: exposure, y/z: ambient strengths, w: default standard specular.
    glm::vec4 lighting_params;

    /// x: radius, y: intensity, z: bias, w: power.
    glm::vec4 ao_params;

    /// x: thickness, y: enabled, z/w: reserved.
    glm::vec4 ao_params2;

    /// x/y: environment readiness, z/w: diffuse and specular strength.
    glm::vec4 ibl_params;

    /// x: occlusion strength, y: occlusion power, z: sky visibility, w: enabled.
    glm::vec4 ibl_params2;

    /// x/y: prefilter and BRDF LUT readiness, z: max LOD, w: reserved.
    glm::vec4 ibl_params3;
};

/**
 * @brief Material constants uploaded for one frame.
 *
 * @details
 * Indexed by DrawCall::material_index. Keep this layout in sync with MaterialSSBO blocks in
 * renderer shaders.
 */
struct alignas(16) GpuMaterialData {
    glm::vec4 base_color_factor;

    /// x: metallic, y: roughness, z: alpha, w: alpha cutoff.
    glm::vec4 params0;

    /// x: shading model, y: blend mode, z: texture flags, w: reserved.
    glm::uvec4 params1;
};

} // namespace fr
