/**
 * @file render_extractor.hpp
 * @author Tfoedy
 * @brief High-level ECS to renderer-frame extraction facade.
 */

#pragma once

#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_manager.hpp"
#include "fr/data/world.hpp"

#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_frame.hpp"
#include "fr/renderer/renderer_desc.hpp"

#include "fr/scene/render_scene_extractor.hpp"

namespace fr {

/**
 * @brief Runtime settings required to extract one render frame.
 */
struct RenderExtractDesc {
    F32 aspect_ratio{1.0f};

    RenderPipelineHandle geometry_pipeline{};
    RenderPipelineHandle forward_transparent_pipeline{};
    RenderPipelineHandle shadow_pipeline{};

    RenderDirectionalShadowSettings directional_shadow_settings{};
};

/**
 * @brief Result of one render extraction step.
 */
struct RenderExtractResult {
    RenderCameraDesc camera{};

    RenderSubmitStats geometry_stats{};
    RenderSubmitStats shadow_stats{};

    bool has_main_camera{false};
};

/**
 * @brief Extracts one renderer-facing frame from ECS scene data.
 */
inline RenderExtractResult extract_render_frame(World &world, const AssetManager &assets,
                                                const RenderExtractDesc &desc,
                                                RenderFrameSubmission &out_submission) noexcept {
    FR_ASSERT(desc.aspect_ratio > 0.0f, "RenderExtractDesc::aspect_ratio must be positive");

    out_submission.clear();

    RenderExtractResult result{};

    const RenderCameraData cam =
        RenderSceneExtractor::extract_camera_data(world, desc.aspect_ratio);

    result.camera.view_proj = cam.view_proj;
    result.camera.position = cam.position;
    result.camera.forward = cam.forward;

    result.has_main_camera = cam.found;

    const RenderPipelineHandle geometry_pipeline = desc.geometry_pipeline;
    const RenderPipelineHandle forward_transparent_pipeline = desc.forward_transparent_pipeline;
    const RenderPipelineHandle shadow_pipeline = desc.shadow_pipeline;

    FR_ASSERT(geometry_pipeline.is_valid(), "RenderExtractDesc::geometry_pipeline must be valid");
    FR_ASSERT(forward_transparent_pipeline.is_valid(),
              "RenderExtractDesc::forward_transparent_pipeline must be valid");
    FR_ASSERT(shadow_pipeline.is_valid(), "RenderExtractDesc::shadow_pipeline must be valid");

    result.geometry_stats = RenderSceneExtractor::submit_meshes(
        world, out_submission, assets, geometry_pipeline, forward_transparent_pipeline,
        cam.view_proj, cam.position, cam.forward);

    result.shadow_stats =
        RenderSceneExtractor::submit_shadow_casters(world, out_submission, assets, shadow_pipeline);

    RenderSceneExtractor::submit_lights(world, out_submission);

    RenderSceneExtractor::submit_directional_lights(world, out_submission, cam.position,
                                                    cam.forward, desc.directional_shadow_settings);

    out_submission.sort();

    return result;
}

} // namespace fr
