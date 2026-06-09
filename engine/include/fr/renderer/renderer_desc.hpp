/**
 * @file renderer_desc.hpp
 * @author Tfoedy
 * @brief Per-frame renderer input descriptions.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"
#include "fr/renderer/renderer_constants.hpp"

#include <glm/glm.hpp>

namespace fr {

enum class RenderDebugMode : U32 {
    Final = 0,
    Albedo = 1,
    Normal = 2,
    MetallicSpecular = 3,
    Roughness = 4,
    AmbientOcclusion = 5,
    ShadingModel = 6,
    Shadow = 7,
    Hbao = 8,
};

struct RendererLimits {
    USize max_instances{MAX_INSTANCES};

    USize max_point_lights{MAX_POINT_LIGHTS};
    USize max_spot_lights{MAX_RENDER_SPOT_LIGHTS};
    USize max_dir_lights{MAX_DIR_LIGHTS};

    USize max_point_shadows{MAX_POINT_SHADOWS};
    USize max_spot_shadows{MAX_SPOT_SHADOWS};
};

struct RendererCreateDesc {
    RendererLimits limits{};
};

struct RenderViewportDesc {
    U32 width{0};
    U32 height{0};
};

struct RenderCameraDesc {
    glm::mat4 view_proj{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
};

struct RenderPipelineSet {
    RenderPipelineHandle lighting{};
    RenderPipelineHandle forward_transparent{};
    RenderPipelineHandle present{};

    RenderPipelineHandle shadow{};
    RenderPipelineHandle point_shadow{};
    RenderPipelineHandle spot_shadow{};

    RenderPipelineHandle hbao{};
    RenderPipelineHandle hbao_blur{};

    RenderPipelineHandle equirect_to_cube{};
    RenderPipelineHandle irradiance{};
    RenderPipelineHandle prefilter_env{};
    RenderPipelineHandle brdf_lut{};
};

struct RenderLightingSettings {
    F32 exposure{1.0f};
    F32 pbr_ambient_strength{0.01f};
    F32 standard_ambient_strength{0.035f};
    F32 standard_specular_default{0.25f};
};

struct RenderAoSettings {
    bool enabled{false};

    F32 radius{1.5f};
    F32 intensity{1.2f};
    F32 bias{0.05f};
    F32 power{1.5f};
    F32 thickness{1.0f};
};

struct RenderIblSettings {
    bool enabled{true};

    F32 diffuse_strength{0.10f};
    F32 specular_strength{1.0f};

    F32 occlusion_strength{1.0f};
    F32 occlusion_power{2.0f};
    F32 sky_visibility_strength{0.75f};
};

struct RenderDebugSettings {
    RenderDebugMode mode{RenderDebugMode::Final};
    U32 flags{0};
};

struct RenderFrameDesc {
    const RenderQueue *geom_queue{nullptr};
    const RenderQueue *shadow_queue{nullptr};

    RenderViewportDesc viewport{};
    RenderCameraDesc camera{};
    RenderPipelineSet pipelines{};

    TextureHandle skybox_map{};

    RenderLightingSettings lighting{};
    RenderAoSettings ao{};
    RenderIblSettings ibl{};
    RenderDebugSettings debug{};
};

} // namespace fr
