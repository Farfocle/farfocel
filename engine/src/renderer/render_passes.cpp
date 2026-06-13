/**
 * @file render_passes.cpp
 * @author Tfoedy
 * @brief Built-in renderer pass implementations.
 */

#include "fr/renderer/render_passes.hpp"

#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/renderer/render_bindings.hpp"

namespace fr::render_pass {

void execute_ibl_environment(CommandBuffer *cmd, const IblEnvironmentPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "IblEnvironmentPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "IblEnvironmentPassDesc::pipelines must be non-null");

    IblResources &ibl = desc.resources->ibl;
    const RendererPipelineSet &pipelines = *desc.pipelines;

    if (!desc.source.is_valid() || !ibl.environment.is_valid() ||
        !pipelines.equirect_to_cube.is_valid()) {
        return;
    }

    if (ibl.environment_ready && ibl.source.key == desc.source.key) {
        return;
    }

    cmd->set_pipeline(pipelines.equirect_to_cube);
    cmd->bind_texture(desc.source, fr::bindings::IBL_SOURCE_MAP);

    for (U32 face = 0; face < 6; ++face) {
        RenderAttachment color_attachment{};
        color_attachment.texture = ibl.environment;
        color_attachment.layer = face;
        color_attachment.mip_level = 0;

        RenderAttachment color_targets[] = {color_attachment};

        cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1),
                                  RenderAttachment{});
        cmd->set_viewport(0, 0, ibl.environment_size, ibl.environment_size);

        struct IblPushConstants {
            U32 transform_index;
            U32 unused0;
            U32 face_idx;
            U32 unused1;
        };

        IblPushConstants push_data{0, 0, face, 0};

        cmd->set_push_constants(
            Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

        cmd->draw_arrays(3, 0);
        cmd->end_render_pass();
    }

    ibl.source = desc.source;
    ibl.environment_ready = true;
    ibl.irradiance_ready = false;
    ibl.prefiltered_ready = false;
}

void execute_ibl_irradiance(CommandBuffer *cmd, const IblIrradiancePassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "IblIrradiancePassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "IblIrradiancePassDesc::pipelines must be non-null");

    IblResources &ibl = desc.resources->ibl;
    const RendererPipelineSet &pipelines = *desc.pipelines;

    if (!ibl.environment_ready || ibl.irradiance_ready || !ibl.environment.is_valid() ||
        !ibl.irradiance.is_valid() || !pipelines.irradiance.is_valid()) {
        return;
    }

    cmd->set_pipeline(pipelines.irradiance);
    cmd->bind_texture(ibl.environment, fr::bindings::IBL_SOURCE_MAP);

    for (U32 face = 0; face < 6; ++face) {
        RenderAttachment color_attachment{};
        color_attachment.texture = ibl.irradiance;
        color_attachment.layer = face;
        color_attachment.mip_level = 0;

        RenderAttachment color_targets[] = {color_attachment};

        cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1),
                                  RenderAttachment{});
        cmd->set_viewport(0, 0, ibl.irradiance_size, ibl.irradiance_size);

        struct IblPushConstants {
            U32 transform_index;
            U32 unused0;
            U32 face_idx;
            U32 unused1;
        };

        IblPushConstants push_data{0, 0, face, 0};

        cmd->set_push_constants(
            Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

        cmd->draw_arrays(3, 0);
        cmd->end_render_pass();
    }

    ibl.irradiance_ready = true;
}

void execute_ibl_prefilter(CommandBuffer *cmd, const IblPrefilterPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "IblPrefilterPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "IblPrefilterPassDesc::pipelines must be non-null");

    IblResources &ibl = desc.resources->ibl;
    const RendererPipelineSet &pipelines = *desc.pipelines;

    if (!ibl.environment_ready || ibl.prefiltered_ready || !ibl.environment.is_valid() ||
        !ibl.prefiltered.is_valid() || !pipelines.prefilter_env.is_valid()) {
        return;
    }

    cmd->set_pipeline(pipelines.prefilter_env);
    cmd->bind_texture(ibl.environment, fr::bindings::IBL_SOURCE_MAP);

    for (U32 mip = 0; mip < ibl.prefiltered_mips; ++mip) {
        const U32 mip_size = fr::math::max<U32>(1, ibl.prefiltered_size >> mip);

        for (U32 face = 0; face < 6; ++face) {
            RenderAttachment color_attachment{};
            color_attachment.texture = ibl.prefiltered;
            color_attachment.layer = face;
            color_attachment.mip_level = mip;

            RenderAttachment color_targets[] = {color_attachment};

            cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1),
                                      RenderAttachment{});
            cmd->set_viewport(0, 0, mip_size, mip_size);

            struct IblPushConstants {
                U32 transform_index;
                U32 unused0;
                U32 face_idx;
                U32 mip_idx;
            };

            IblPushConstants push_data{0, 0, face, mip};

            cmd->set_push_constants(
                Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

            cmd->draw_arrays(3, 0);
            cmd->end_render_pass();
        }
    }

    ibl.prefiltered_ready = true;
}

void execute_ibl_brdf_lut(CommandBuffer *cmd, const IblBrdfLutPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "IblBrdfLutPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "IblBrdfLutPassDesc::pipelines must be non-null");

    IblResources &ibl = desc.resources->ibl;
    const RendererPipelineSet &pipelines = *desc.pipelines;

    if (ibl.brdf_lut_ready || !ibl.brdf_lut.is_valid() || !pipelines.brdf_lut.is_valid()) {
        return;
    }

    TextureHandle color_targets[] = {ibl.brdf_lut};

    cmd->begin_render_pass(Slice<const TextureHandle>(color_targets, 1), TextureHandle{});
    cmd->set_viewport(0, 0, ibl.brdf_lut_size, ibl.brdf_lut_size);
    cmd->set_pipeline(pipelines.brdf_lut);
    cmd->draw_arrays(3, 0);
    cmd->end_render_pass();

    ibl.brdf_lut_ready = true;
}

void execute_directional_shadow(CommandBuffer *cmd,
                                const DirectionalShadowPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "DirectionalShadowPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "DirectionalShadowPassDesc::pipelines must be non-null");
    FR_ASSERT(desc.submission, "DirectionalShadowPassDesc::submission must be non-null");

    RendererGlobalBuffers &global = desc.resources->global;
    RendererFallbackTextures &fallback = desc.resources->fallback;
    DirectionalShadowResources &shadow = desc.resources->shadow;
    const RendererPipelineSet &pipelines = *desc.pipelines;
    const RenderFrameSubmission &submission = *desc.submission;

    if (!shadow.map.is_valid() || !pipelines.shadow.is_valid() ||
        submission.directional_lights.is_empty() || submission.draws.shadow.is_empty()) {
        return;
    }

    cmd->begin_render_pass(Slice<const TextureHandle>(), shadow.map);
    cmd->set_pipeline(pipelines.shadow);

    cmd->bind_storage_buffer(global.shadow_transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(global.dir_lights_ssbo, fr::bindings::SSBO_DIR_LIGHTS);
    cmd->bind_storage_buffer(global.materials_ssbo, fr::bindings::SSBO_MATERIALS);

    TextureHandle curr_albedo{};

    for (U32 cascade = 0; cascade < static_cast<U32>(DIRECTIONAL_CASCADE_COUNT); ++cascade) {
        const U32 cascade_size = shadow.size / 2;
        const U32 vx = (cascade % 2) * cascade_size;
        const U32 vy = (cascade / 2) * cascade_size;

        cmd->set_viewport(vx, vy, cascade_size, cascade_size);

        BufferHandle curr_vbo{};
        BufferHandle curr_ibo{};

        for (const DrawCall &call : submission.draws.shadow) {
            const RenderMaterialPacket &material = submission.materials[call.material_index];

            if (call.vbo.key != curr_vbo.key) {
                cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                curr_vbo = call.vbo;
            }

            if (call.ibo.key != curr_ibo.key) {
                cmd->bind_index_buffer(call.ibo);
                curr_ibo = call.ibo;
            }

            TextureHandle albedo = material.albedo.is_valid() ? material.albedo : fallback.white;

            if (albedo.key != curr_albedo.key) {
                cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
                curr_albedo = albedo;
            }

            struct ShadowPushConstants {
                U32 transform_index;
                U32 material_index;
                U32 cascade_idx;
                U32 shadow_idx;
            };

            ShadowPushConstants push_data{
                call.transform_index,
                call.material_index,
                cascade,
                0,
            };

            cmd->set_push_constants(
                Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

            cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
        }
    }

    cmd->end_render_pass();
}

void execute_point_shadow(CommandBuffer *cmd, const PointShadowPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "PointShadowPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "PointShadowPassDesc::pipelines must be non-null");
    FR_ASSERT(desc.submission, "PointShadowPassDesc::submission must be non-null");

    RendererGlobalBuffers &global = desc.resources->global;
    RendererFallbackTextures &fallback = desc.resources->fallback;
    PointShadowResources &point_shadow_resources = desc.resources->point_shadows;
    const RendererPipelineSet &pipelines = *desc.pipelines;
    const RenderFrameSubmission &submission = *desc.submission;

    Slice<const PointShadowData> point_shadows = submission.point_shadows.slice();

    if (point_shadows.is_empty() || desc.limits.max_point_shadows == 0 ||
        !pipelines.point_shadow.is_valid()) {
        return;
    }

    const USize shadow_count = fr::math::min(point_shadows.size(), desc.limits.max_point_shadows);

    cmd->set_pipeline(pipelines.point_shadow);
    cmd->bind_storage_buffer(global.shadow_transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(global.point_shadows_ssbo, fr::bindings::SSBO_POINT_SHADOWS);
    cmd->bind_storage_buffer(global.materials_ssbo, fr::bindings::SSBO_MATERIALS);

    TextureHandle curr_albedo{};

    for (USize shadow_idx = 0; shadow_idx < shadow_count; ++shadow_idx) {
        TextureHandle cube_map = point_shadow_resources.cube_maps[shadow_idx];

        if (!cube_map.is_valid()) {
            continue;
        }

        for (U32 face = 0; face < POINT_SHADOW_FACE_COUNT; ++face) {
            RenderAttachment depth_attachment{};
            depth_attachment.texture = cube_map;
            depth_attachment.layer = face;
            depth_attachment.mip_level = 0;

            cmd->begin_render_pass_ex(Slice<const RenderAttachment>(), depth_attachment);
            cmd->set_viewport(0, 0, point_shadow_resources.size, point_shadow_resources.size);

            BufferHandle curr_vbo{};
            BufferHandle curr_ibo{};

            for (const DrawCall &call : submission.draws.shadow) {
                const RenderMaterialPacket &material = submission.materials[call.material_index];

                if (call.vbo.key != curr_vbo.key) {
                    cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                    curr_vbo = call.vbo;
                }

                if (call.ibo.key != curr_ibo.key) {
                    cmd->bind_index_buffer(call.ibo);
                    curr_ibo = call.ibo;
                }

                TextureHandle albedo =
                    material.albedo.is_valid() ? material.albedo : fallback.white;

                if (albedo.key != curr_albedo.key) {
                    cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
                    curr_albedo = albedo;
                }

                struct PointShadowPushConstants {
                    U32 transform_index;
                    U32 material_index;
                    U32 face_idx;
                    U32 shadow_idx;
                };

                PointShadowPushConstants push_data{
                    call.transform_index,
                    call.material_index,
                    face,
                    static_cast<U32>(shadow_idx),
                };

                cmd->set_push_constants(Slice<const Byte>(
                    reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

                cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
            }

            cmd->end_render_pass();
        }
    }
}

void execute_spot_shadow(CommandBuffer *cmd, const SpotShadowPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "SpotShadowPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "SpotShadowPassDesc::pipelines must be non-null");
    FR_ASSERT(desc.submission, "SpotShadowPassDesc::submission must be non-null");

    RendererGlobalBuffers &global = desc.resources->global;
    RendererFallbackTextures &fallback = desc.resources->fallback;
    SpotShadowResources &spot_shadow_resources = desc.resources->spot_shadows;
    const RendererPipelineSet &pipelines = *desc.pipelines;
    const RenderFrameSubmission &submission = *desc.submission;

    Slice<const SpotShadowData> spot_shadows = submission.spot_shadows.slice();

    if (spot_shadows.is_empty() || desc.limits.max_spot_shadows == 0 ||
        !spot_shadow_resources.atlas.is_valid() || !pipelines.spot_shadow.is_valid()) {
        return;
    }

    const USize shadow_count = fr::math::min(spot_shadows.size(), desc.limits.max_spot_shadows);

    cmd->begin_render_pass(Slice<const TextureHandle>(), spot_shadow_resources.atlas);
    cmd->set_pipeline(pipelines.spot_shadow);

    cmd->bind_storage_buffer(global.shadow_transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(global.spot_shadows_ssbo, fr::bindings::SSBO_SPOT_SHADOWS);
    cmd->bind_storage_buffer(global.materials_ssbo, fr::bindings::SSBO_MATERIALS);

    TextureHandle curr_albedo{};

    for (USize shadow_idx = 0; shadow_idx < shadow_count; ++shadow_idx) {
        const U32 tile = static_cast<U32>(shadow_idx);
        const U32 col = tile % 2;
        const U32 row = tile / 2;

        const U32 vx = col * spot_shadow_resources.tile_size;
        const U32 vy = row * spot_shadow_resources.tile_size;

        cmd->set_viewport(vx, vy, spot_shadow_resources.tile_size, spot_shadow_resources.tile_size);

        BufferHandle curr_vbo{};
        BufferHandle curr_ibo{};

        for (const DrawCall &call : submission.draws.shadow) {
            const RenderMaterialPacket &material = submission.materials[call.material_index];

            if (call.vbo.key != curr_vbo.key) {
                cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                curr_vbo = call.vbo;
            }

            if (call.ibo.key != curr_ibo.key) {
                cmd->bind_index_buffer(call.ibo);
                curr_ibo = call.ibo;
            }

            TextureHandle albedo = material.albedo.is_valid() ? material.albedo : fallback.white;

            if (albedo.key != curr_albedo.key) {
                cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
                curr_albedo = albedo;
            }

            struct SpotShadowPushConstants {
                U32 transform_index;
                U32 material_index;
                U32 unused;
                U32 shadow_idx;
            };

            SpotShadowPushConstants push_data{
                call.transform_index,
                call.material_index,
                0,
                static_cast<U32>(shadow_idx),
            };

            cmd->set_push_constants(
                Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

            cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
        }
    }

    cmd->end_render_pass();
}

void execute_geometry(CommandBuffer *cmd, const GeometryPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "GeometryPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "GeometryPassDesc::pipelines must be non-null");
    FR_ASSERT(desc.submission, "GeometryPassDesc::submission must be non-null");

    RendererGlobalBuffers &global = desc.resources->global;
    RendererFallbackTextures &fallback = desc.resources->fallback;
    GBufferTargets &gbuffer = desc.resources->gbuffer;
    const RenderFrameSubmission &submission = *desc.submission;

    TextureHandle color_targets[] = {
        gbuffer.albedo,
        gbuffer.normal,
        gbuffer.extra,
    };

    cmd->begin_render_pass(Slice<const TextureHandle>(color_targets, 3), gbuffer.depth);
    cmd->set_viewport(0, 0, desc.width, desc.height);

    cmd->bind_storage_buffer(global.transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(global.camera_ssbo, fr::bindings::SSBO_CAMERA);
    cmd->bind_storage_buffer(global.materials_ssbo, fr::bindings::SSBO_MATERIALS);

    RenderPipelineHandle curr_pipe{};
    BufferHandle curr_vbo{};
    BufferHandle curr_ibo{};
    TextureHandle curr_textures[3]{};

    auto draw_list = [&](const DynamicArray<DrawCall> &draws) noexcept {
        for (const DrawCall &call : draws) {
            const RenderMaterialPacket &material = submission.materials[call.material_index];

            if (call.pipe.key != curr_pipe.key) {
                cmd->set_pipeline(call.pipe);
                curr_pipe = call.pipe;
            }

            if (call.vbo.key != curr_vbo.key) {
                cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                curr_vbo = call.vbo;
            }

            if (call.ibo.key != curr_ibo.key) {
                cmd->bind_index_buffer(call.ibo);
                curr_ibo = call.ibo;
            }

            TextureHandle albedo = material.albedo.is_valid() ? material.albedo : fallback.white;
            TextureHandle normal = material.normal.is_valid() ? material.normal : fallback.normal;
            TextureHandle extra = material.extra.is_valid() ? material.extra : fallback.material;

            if (albedo.key != curr_textures[0].key) {
                cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
                curr_textures[0] = albedo;
            }

            if (normal.key != curr_textures[1].key) {
                cmd->bind_texture(normal, fr::bindings::TEX_NORMAL);
                curr_textures[1] = normal;
            }

            if (extra.key != curr_textures[2].key) {
                cmd->bind_texture(extra, fr::bindings::TEX_EXTRA);
                curr_textures[2] = extra;
            }

            struct DrawPushConstants {
                U32 transform_index;
                U32 material_index;
                U32 cascade_idx;
                U32 shadow_idx;
            };

            DrawPushConstants push_data{
                call.transform_index,
                call.material_index,
                0,
                0,
            };

            cmd->set_push_constants(
                Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

            cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
        }
    };

    draw_list(submission.draws.opaque);
    draw_list(submission.draws.masked);

    cmd->end_render_pass();
}

void execute_hbao(CommandBuffer *cmd, const HbaoPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "HbaoPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "HbaoPassDesc::pipelines must be non-null");
    FR_ASSERT(desc.frame, "HbaoPassDesc::frame must be non-null");

    RendererGlobalBuffers &global = desc.resources->global;
    GBufferTargets &gbuffer = desc.resources->gbuffer;
    AmbientOcclusionResources &ao = desc.resources->ao;
    const RendererPipelineSet &pipelines = *desc.pipelines;
    const RenderFrameDesc &frame = *desc.frame;

    if (!ao.target.is_valid() || !pipelines.hbao.is_valid()) {
        return;
    }

    TextureHandle color_targets[] = {ao.target};

    cmd->begin_render_pass(Slice<const TextureHandle>(color_targets, 1), TextureHandle{});
    cmd->set_viewport(0, 0, frame.viewport.width, frame.viewport.height);
    cmd->set_pipeline(pipelines.hbao);

    cmd->bind_texture(gbuffer.normal, fr::bindings::HBAO_NORMAL);
    cmd->bind_texture(gbuffer.depth, fr::bindings::HBAO_DEPTH);
    cmd->bind_storage_buffer(global.camera_ssbo, fr::bindings::SSBO_CAMERA);

    cmd->draw_arrays(3, 0);
    cmd->end_render_pass();
}

void execute_lighting(CommandBuffer *cmd, const LightingPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "LightingPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "LightingPassDesc::pipelines must be non-null");
    FR_ASSERT(desc.frame, "LightingPassDesc::frame must be non-null");

    RendererGlobalBuffers &global = desc.resources->global;
    RendererFallbackTextures &fallback = desc.resources->fallback;

    GBufferTargets &gbuffer = desc.resources->gbuffer;
    FinalColorTarget &final = desc.resources->final;
    AmbientOcclusionResources &ao = desc.resources->ao;

    DirectionalShadowResources &shadow = desc.resources->shadow;
    PointShadowResources &point_shadows = desc.resources->point_shadows;
    SpotShadowResources &spot_shadows = desc.resources->spot_shadows;
    IblResources &ibl = desc.resources->ibl;

    const RendererPipelineSet &pipelines = *desc.pipelines;
    const RenderFrameDesc &frame = *desc.frame;

    if (!final.color.is_valid() || !pipelines.lighting.is_valid()) {
        return;
    }

    RenderAttachment color_attachment{};
    color_attachment.texture = final.color;
    color_attachment.layer = 0;
    color_attachment.mip_level = 0;
    color_attachment.load_op = RenderLoadOp::Clear;

    RenderAttachment color_targets[] = {
        color_attachment,
    };

    cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1), RenderAttachment{});

    cmd->set_viewport(0, 0, frame.viewport.width, frame.viewport.height);
    cmd->set_pipeline(pipelines.lighting);

    cmd->bind_texture(gbuffer.albedo, fr::bindings::GBUFFER_ALBEDO);
    cmd->bind_texture(gbuffer.normal, fr::bindings::GBUFFER_NORMAL);
    cmd->bind_texture(gbuffer.extra, fr::bindings::GBUFFER_EXTRA);
    cmd->bind_texture(gbuffer.depth, fr::bindings::GBUFFER_DEPTH);
    cmd->bind_texture(shadow.map, fr::bindings::SHADOW_MAP);

    if (ibl.environment.is_valid()) {
        cmd->bind_texture(ibl.environment, fr::bindings::IBL_ENVIRONMENT);
    }

    TextureHandle ao_map = (frame.ao.enabled && ao.target.is_valid()) ? ao.target : fallback.white;

    cmd->bind_texture(ao_map, fr::bindings::HBAO_MAP);

    for (USize i = 0; i < desc.limits.max_point_shadows; ++i) {
        if (point_shadows.cube_maps[i].is_valid()) {
            cmd->bind_texture(point_shadows.cube_maps[i],
                              fr::bindings::POINT_SHADOW_MAP_BASE + static_cast<U32>(i));
        }
    }

    if (spot_shadows.atlas.is_valid()) {
        cmd->bind_texture(spot_shadows.atlas, fr::bindings::SPOT_SHADOW_MAP);
    }

    if (ibl.irradiance.is_valid()) {
        cmd->bind_texture(ibl.irradiance, fr::bindings::IBL_IRRADIANCE);
    }

    if (ibl.prefiltered.is_valid()) {
        cmd->bind_texture(ibl.prefiltered, fr::bindings::IBL_PREFILTERED);
    }

    if (ibl.brdf_lut.is_valid()) {
        cmd->bind_texture(ibl.brdf_lut, fr::bindings::IBL_BRDF_LUT);
    }

    cmd->bind_storage_buffer(global.camera_ssbo, fr::bindings::SSBO_CAMERA);
    cmd->bind_storage_buffer(global.point_lights_ssbo, fr::bindings::SSBO_POINT_LIGHTS);
    cmd->bind_storage_buffer(global.dir_lights_ssbo, fr::bindings::SSBO_DIR_LIGHTS);
    cmd->bind_storage_buffer(global.point_shadows_ssbo, fr::bindings::SSBO_POINT_SHADOWS);
    cmd->bind_storage_buffer(global.spot_lights_ssbo, fr::bindings::SSBO_SPOT_LIGHTS);
    cmd->bind_storage_buffer(global.spot_shadows_ssbo, fr::bindings::SSBO_SPOT_SHADOWS);

    cmd->draw_arrays(3, 0);
    cmd->end_render_pass();
}

void execute_forward_transparent(CommandBuffer *cmd,
                                 const ForwardTransparentPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "ForwardTransparentPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "ForwardTransparentPassDesc::pipelines must be non-null");
    FR_ASSERT(desc.frame, "ForwardTransparentPassDesc::frame must be non-null");

    RendererGlobalBuffers &global = desc.resources->global;
    RendererFallbackTextures &fallback = desc.resources->fallback;

    GBufferTargets &gbuffer = desc.resources->gbuffer;
    FinalColorTarget &final = desc.resources->final;

    DirectionalShadowResources &shadow = desc.resources->shadow;
    PointShadowResources &point_shadows = desc.resources->point_shadows;
    SpotShadowResources &spot_shadows = desc.resources->spot_shadows;
    IblResources &ibl = desc.resources->ibl;

    const RendererPipelineSet &pipelines = *desc.pipelines;
    const RenderFrameDesc &frame = *desc.frame;
    const RenderFrameSubmission &submission = *frame.submission;

    if (submission.draws.transparent.is_empty() || !final.color.is_valid() ||
        !gbuffer.depth.is_valid() || !pipelines.forward_transparent.is_valid()) {
        return;
    }

    RenderAttachment color_attachment{};
    color_attachment.texture = final.color;
    color_attachment.layer = 0;
    color_attachment.mip_level = 0;
    color_attachment.load_op = RenderLoadOp::Load;

    RenderAttachment depth_attachment{};
    depth_attachment.texture = gbuffer.depth;
    depth_attachment.layer = 0;
    depth_attachment.mip_level = 0;
    depth_attachment.load_op = RenderLoadOp::Load;

    RenderAttachment color_targets[] = {
        color_attachment,
    };

    cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1), depth_attachment);

    cmd->set_viewport(0, 0, frame.viewport.width, frame.viewport.height);

    cmd->bind_storage_buffer(global.transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(global.camera_ssbo, fr::bindings::SSBO_CAMERA);
    cmd->bind_storage_buffer(global.materials_ssbo, fr::bindings::SSBO_MATERIALS);

    cmd->bind_storage_buffer(global.point_lights_ssbo, fr::bindings::SSBO_POINT_LIGHTS);
    cmd->bind_storage_buffer(global.dir_lights_ssbo, fr::bindings::SSBO_DIR_LIGHTS);
    cmd->bind_storage_buffer(global.point_shadows_ssbo, fr::bindings::SSBO_POINT_SHADOWS);
    cmd->bind_storage_buffer(global.spot_lights_ssbo, fr::bindings::SSBO_SPOT_LIGHTS);
    cmd->bind_storage_buffer(global.spot_shadows_ssbo, fr::bindings::SSBO_SPOT_SHADOWS);

    if (shadow.map.is_valid()) {
        cmd->bind_texture(shadow.map, fr::bindings::SHADOW_MAP);
    }

    for (USize i = 0; i < desc.limits.max_point_shadows; ++i) {
        if (point_shadows.cube_maps[i].is_valid()) {
            cmd->bind_texture(point_shadows.cube_maps[i],
                              fr::bindings::POINT_SHADOW_MAP_BASE + static_cast<U32>(i));
        }
    }

    if (spot_shadows.atlas.is_valid()) {
        cmd->bind_texture(spot_shadows.atlas, fr::bindings::SPOT_SHADOW_MAP);
    }

    if (ibl.environment.is_valid()) {
        cmd->bind_texture(ibl.environment, fr::bindings::IBL_ENVIRONMENT);
    }

    if (ibl.irradiance.is_valid()) {
        cmd->bind_texture(ibl.irradiance, fr::bindings::IBL_IRRADIANCE);
    }

    if (ibl.prefiltered.is_valid()) {
        cmd->bind_texture(ibl.prefiltered, fr::bindings::IBL_PREFILTERED);
    }

    if (ibl.brdf_lut.is_valid()) {
        cmd->bind_texture(ibl.brdf_lut, fr::bindings::IBL_BRDF_LUT);
    }

    RenderPipelineHandle curr_pipe{};
    BufferHandle curr_vbo{};
    BufferHandle curr_ibo{};
    TextureHandle curr_textures[3]{};

    for (const DrawCall &call : submission.draws.transparent) {
        const RenderMaterialPacket &material = submission.materials[call.material_index];

        if (call.pipe.key != curr_pipe.key) {
            cmd->set_pipeline(call.pipe);
            curr_pipe = call.pipe;
        }

        if (call.vbo.key != curr_vbo.key) {
            cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
            curr_vbo = call.vbo;
        }

        if (call.ibo.key != curr_ibo.key) {
            cmd->bind_index_buffer(call.ibo);
            curr_ibo = call.ibo;
        }

        TextureHandle albedo = material.albedo.is_valid() ? material.albedo : fallback.white;
        TextureHandle normal = material.normal.is_valid() ? material.normal : fallback.normal;
        TextureHandle extra = material.extra.is_valid() ? material.extra : fallback.material;

        if (albedo.key != curr_textures[0].key) {
            cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
            curr_textures[0] = albedo;
        }

        if (normal.key != curr_textures[1].key) {
            cmd->bind_texture(normal, fr::bindings::TEX_NORMAL);
            curr_textures[1] = normal;
        }

        if (extra.key != curr_textures[2].key) {
            cmd->bind_texture(extra, fr::bindings::TEX_EXTRA);
            curr_textures[2] = extra;
        }

        struct DrawPushConstants {
            U32 transform_index;
            U32 material_index;
            U32 cascade_idx;
            U32 shadow_idx;
        };

        DrawPushConstants push_data{
            call.transform_index,
            call.material_index,
            0,
            0,
        };

        cmd->set_push_constants(
            Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

        cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
    }

    cmd->end_render_pass();
}

void execute_present(CommandBuffer *cmd, const PresentPassDesc &desc) noexcept {
    FR_ASSERT(cmd, "CommandBuffer must be non-null");
    FR_ASSERT(desc.resources, "PresentPassDesc::resources must be non-null");
    FR_ASSERT(desc.pipelines, "PresentPassDesc::pipelines must be non-null");

    FinalColorTarget &final = desc.resources->final;
    const RendererPipelineSet &pipelines = *desc.pipelines;

    if (!final.color.is_valid() || !pipelines.present.is_valid()) {
        return;
    }

    cmd->begin_render_pass(Slice<const TextureHandle>(), TextureHandle{});
    cmd->set_viewport(0, 0, final.width, final.height);
    cmd->set_pipeline(pipelines.present);

    cmd->bind_texture(final.color, fr::bindings::PRESENT_COLOR);

    cmd->draw_arrays(3, 0);
    cmd->end_render_pass();
}

} // namespace fr::render_pass
