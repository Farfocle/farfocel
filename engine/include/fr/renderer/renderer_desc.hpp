/**
 * @file renderer_desc.hpp
 * @author Tfoedy
 * @brief Renderer setup and per-frame input descriptions.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/math.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_frame.hpp"
#include "fr/renderer/renderer_limits.hpp"
#include "fr/renderer/renderer_pipelines.hpp"

namespace fr {

/**
 * @brief Built-in renderer debug output mode.
 */
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

/**
 * @brief Runtime limits for renderer-owned GPU buffers.
 */
struct RendererLimits {
    USize max_instances{MAX_INSTANCES};
    USize max_materials{MAX_RENDER_MATERIALS};

    USize max_point_lights{MAX_POINT_LIGHTS};
    USize max_spot_lights{MAX_RENDER_SPOT_LIGHTS};
    USize max_dir_lights{MAX_DIR_LIGHTS};

    USize max_point_shadows{MAX_POINT_SHADOWS};
    USize max_spot_shadows{MAX_SPOT_SHADOWS};
};

/**
 * @brief Renderer construction parameters.
 *
 * @details
 * Renderer consumes an existing pipeline set. Shader loading and pipeline creation are handled
 * by the asset and pipeline-cache layers.
 */
struct RendererCreateDesc {
    Alloc *alloc{nullptr};
    RendererLimits limits{};
    RendererPipelineSet pipelines{};
};

/**
 * @brief Frame viewport size in pixels.
 */
struct RenderViewportDesc {
    U32 width{0};
    U32 height{0};
};

/**
 * @brief Camera data used by renderer passes.
 */
struct RenderCameraDesc {
    Mat4 view_proj{1.0f};
    Vec3 position{0.0f};
    Vec3 forward{0.0f, 0.0f, -1.0f};
};

/**
 * @brief Global lighting controls for one frame.
 */
struct RenderLightingSettings {
    F32 exposure{1.0f};
    F32 pbr_ambient_strength{0.01f};
    F32 standard_ambient_strength{0.035f};
    F32 standard_specular_default{0.25f};
};

/**
 * @brief Ambient occlusion controls for one frame.
 */
struct RenderAmbientOcclusionSettings {
    bool enabled{false};

    F32 radius{1.5f};
    F32 intensity{1.2f};
    F32 bias{0.05f};
    F32 power{1.5f};
    F32 thickness{1.0f};
};

/**
 * @brief Image-based lighting controls for one frame.
 */
struct RenderIblSettings {
    bool enabled{true};

    F32 diffuse_strength{0.10f};
    F32 specular_strength{1.0f};

    F32 occlusion_strength{1.0f};
    F32 occlusion_power{2.0f};
    F32 sky_visibility_strength{0.75f};
};

/**
 * @brief Debug output controls for one frame.
 */
struct RenderDebugSettings {
    RenderDebugMode mode{RenderDebugMode::Final};
    U32 flags{0};
};

/**
 * @brief Per-frame renderer input.
 *
 * @details
 * Renderer consumes this data during Renderer::render() and does not retain pointers to it after
 * the call returns.
 */
struct RenderFrameDesc {
    const RenderFrameSubmission *submission{nullptr};

    RenderViewportDesc viewport{};
    RenderCameraDesc camera{};

    /// @brief Equirectangular environment source used for IBL generation.
    TextureHandle environment_source{};

    RenderLightingSettings lighting{};
    RenderAmbientOcclusionSettings ao{};
    RenderIblSettings ibl{};
    RenderDebugSettings debug{};
};

} // namespace fr
