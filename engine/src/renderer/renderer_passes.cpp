/**
 * @file renderer_passes.cpp
 * @author Tfoedy
 * @brief Deferred renderer pass implementations.
 */

#include "fr/renderer/renderer.hpp"

#include "fr/renderer/render_bindings.hpp"
#include "fr/renderer/renderer_constants.hpp"

#include <algorithm>

namespace fr {

void Renderer::execute_ibl_environment_pass(CommandBuffer *cmd, TextureHandle source,
                                            RenderPipelineHandle pipe) noexcept {
    if (!source.is_valid() || !m_ibl.environment.is_valid() || !pipe.is_valid()) {
        return;
    }

    if (m_ibl.environment_ready && m_ibl.source.key == source.key) {
        return;
    }

    cmd->set_pipeline(pipe);
    cmd->bind_texture(source, fr::bindings::IBL_SOURCE_MAP);

    for (U32 face = 0; face < 6; ++face) {
        RenderAttachment color_attachment{};
        color_attachment.texture = m_ibl.environment;
        color_attachment.layer = face;
        color_attachment.mip_level = 0;

        RenderAttachment color_targets[] = {color_attachment};

        cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1),
                                  RenderAttachment{});
        cmd->set_viewport(0, 0, m_ibl.environment_size, m_ibl.environment_size);

        struct IblPushConstants {
            U32 transform_index;
            U32 shading_model;
            U32 face_idx;
            U32 shadow_idx;
        };

        IblPushConstants push_data{0, 0, face, 0};

        cmd->set_push_constants(
            Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

        cmd->draw_arrays(3, 0);
        cmd->end_render_pass();
    }

    m_ibl.source = source;
    m_ibl.environment_ready = true;
    m_ibl.irradiance_ready = false;
    m_ibl.prefiltered_ready = false;
}

void Renderer::execute_ibl_irradiance_pass(CommandBuffer *cmd, RenderPipelineHandle pipe) noexcept {
    if (!m_ibl.environment_ready || m_ibl.irradiance_ready || !m_ibl.environment.is_valid() ||
        !m_ibl.irradiance.is_valid() || !pipe.is_valid()) {
        return;
    }

    cmd->set_pipeline(pipe);
    cmd->bind_texture(m_ibl.environment, fr::bindings::IBL_SOURCE_MAP);

    for (U32 face = 0; face < 6; ++face) {
        RenderAttachment color_attachment{};
        color_attachment.texture = m_ibl.irradiance;
        color_attachment.layer = face;
        color_attachment.mip_level = 0;

        RenderAttachment color_targets[] = {color_attachment};

        cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1),
                                  RenderAttachment{});
        cmd->set_viewport(0, 0, m_ibl.irradiance_size, m_ibl.irradiance_size);

        struct IblPushConstants {
            U32 transform_index;
            U32 shading_model;
            U32 face_idx;
            U32 shadow_idx;
        };

        IblPushConstants push_data{0, 0, face, 0};

        cmd->set_push_constants(
            Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

        cmd->draw_arrays(3, 0);
        cmd->end_render_pass();
    }

    m_ibl.irradiance_ready = true;
}

void Renderer::execute_ibl_prefilter_pass(CommandBuffer *cmd, RenderPipelineHandle pipe) noexcept {
    if (!m_ibl.environment_ready || m_ibl.prefiltered_ready || !m_ibl.environment.is_valid() ||
        !m_ibl.prefiltered.is_valid() || !pipe.is_valid()) {
        return;
    }

    cmd->set_pipeline(pipe);
    cmd->bind_texture(m_ibl.environment, fr::bindings::IBL_SOURCE_MAP);

    for (U32 mip = 0; mip < m_ibl.prefiltered_mips; ++mip) {
        const U32 mip_size = std::max<U32>(1, m_ibl.prefiltered_size >> mip);

        for (U32 face = 0; face < 6; ++face) {
            RenderAttachment color_attachment{};
            color_attachment.texture = m_ibl.prefiltered;
            color_attachment.layer = face;
            color_attachment.mip_level = mip;

            RenderAttachment color_targets[] = {color_attachment};

            cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1),
                                      RenderAttachment{});
            cmd->set_viewport(0, 0, mip_size, mip_size);

            struct IblPushConstants {
                U32 transform_index;
                U32 shading_model;
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

    m_ibl.prefiltered_ready = true;
}

void Renderer::execute_ibl_brdf_lut_pass(CommandBuffer *cmd, RenderPipelineHandle pipe) noexcept {
    if (m_ibl.brdf_lut_ready || !m_ibl.brdf_lut.is_valid() || !pipe.is_valid()) {
        return;
    }

    TextureHandle color_targets[] = {m_ibl.brdf_lut};

    cmd->begin_render_pass(Slice<const TextureHandle>(color_targets, 1), TextureHandle{});
    cmd->set_viewport(0, 0, m_ibl.brdf_lut_size, m_ibl.brdf_lut_size);
    cmd->set_pipeline(pipe);
    cmd->draw_arrays(3, 0);
    cmd->end_render_pass();

    m_ibl.brdf_lut_ready = true;
}

void Renderer::execute_shadow_pass(CommandBuffer *cmd, const RenderQueue &shadow_queue,
                                   RenderPipelineHandle shadow_pipe) noexcept {
    cmd->begin_render_pass(Slice<const TextureHandle>(), m_shadow.map);
    cmd->set_pipeline(shadow_pipe);

    cmd->bind_storage_buffer(m_global.shadow_transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(m_global.dir_lights_ssbo, fr::bindings::SSBO_DIR_LIGHTS);

    TextureHandle curr_albedo{};

    for (U32 cascade = 0; cascade < 3; ++cascade) {
        const U32 cascade_size = m_shadow.size / 2;
        const U32 vx = (cascade % 2) * cascade_size;
        const U32 vy = (cascade / 2) * cascade_size;

        cmd->set_viewport(vx, vy, cascade_size, cascade_size);

        BufferHandle curr_vbo{};
        BufferHandle curr_ibo{};

        for (const DrawCall &call : shadow_queue.get_calls()) {
            const RenderPassType pass_type = call.key.pass_type();

            if (pass_type != RenderPassType::Opaque && pass_type != RenderPassType::Masked) {
                continue;
            }

            if (call.vbo.key != curr_vbo.key) {
                cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                curr_vbo = call.vbo;
            }

            if (call.ibo.key != curr_ibo.key) {
                cmd->bind_index_buffer(call.ibo);
                curr_ibo = call.ibo;
            }

            TextureHandle albedo = call.albedo_map.is_valid() ? call.albedo_map : m_fallback.white;

            if (albedo.key != curr_albedo.key) {
                cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
                curr_albedo = albedo;
            }

            struct ShadowPushConstants {
                U32 transform_index;
                U32 shading_model;
                U32 cascade_idx;
                U32 shadow_idx;
            };

            ShadowPushConstants push_data{call.transform_index, call.shading_model, cascade, 0};

            cmd->set_push_constants(
                Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

            cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
        }
    }

    cmd->end_render_pass();
}

void Renderer::execute_point_shadow_pass(CommandBuffer *cmd, const RenderQueue &shadow_queue,
                                         const RenderQueue &geom_queue,
                                         RenderPipelineHandle point_shadow_pipe) noexcept {
    Slice<const PointShadowData> point_shadows = geom_queue.get_point_shadows();

    if (point_shadows.is_empty() || m_limits.max_point_shadows == 0) {
        return;
    }

    const USize shadow_count = std::min(point_shadows.size(), m_limits.max_point_shadows);

    cmd->set_pipeline(point_shadow_pipe);
    cmd->bind_storage_buffer(m_global.shadow_transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(m_global.point_shadows_ssbo, fr::bindings::SSBO_POINT_SHADOWS);

    TextureHandle curr_albedo{};

    for (USize shadow_idx = 0; shadow_idx < shadow_count; ++shadow_idx) {
        TextureHandle cube_map = m_point_shadows.cube_maps[shadow_idx];

        if (!cube_map.is_valid()) {
            continue;
        }

        for (U32 face = 0; face < POINT_SHADOW_FACE_COUNT; ++face) {
            RenderAttachment depth_attachment{};
            depth_attachment.texture = cube_map;
            depth_attachment.layer = face;
            depth_attachment.mip_level = 0;

            cmd->begin_render_pass_ex(Slice<const RenderAttachment>(), depth_attachment);
            cmd->set_viewport(0, 0, m_point_shadows.size, m_point_shadows.size);

            BufferHandle curr_vbo{};
            BufferHandle curr_ibo{};

            for (const DrawCall &call : shadow_queue.get_calls()) {
                const RenderPassType pass_type = call.key.pass_type();

                if (pass_type != RenderPassType::Opaque && pass_type != RenderPassType::Masked) {
                    continue;
                }

                if (call.vbo.key != curr_vbo.key) {
                    cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                    curr_vbo = call.vbo;
                }

                if (call.ibo.key != curr_ibo.key) {
                    cmd->bind_index_buffer(call.ibo);
                    curr_ibo = call.ibo;
                }

                TextureHandle albedo =
                    call.albedo_map.is_valid() ? call.albedo_map : m_fallback.white;

                if (albedo.key != curr_albedo.key) {
                    cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
                    curr_albedo = albedo;
                }

                struct PointShadowPushConstants {
                    U32 transform_index;
                    U32 shading_model;
                    U32 face_idx;
                    U32 shadow_idx;
                };

                PointShadowPushConstants push_data{
                    call.transform_index,
                    call.shading_model,
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

void Renderer::execute_spot_shadow_pass(CommandBuffer *cmd, const RenderQueue &shadow_queue,
                                        const RenderQueue &geom_queue,
                                        RenderPipelineHandle spot_shadow_pipe) noexcept {
    Slice<const SpotShadowData> spot_shadows = geom_queue.get_spot_shadows();

    if (spot_shadows.is_empty() || m_limits.max_spot_shadows == 0 ||
        !m_spot_shadows.atlas.is_valid()) {
        return;
    }

    const USize shadow_count = std::min(spot_shadows.size(), m_limits.max_spot_shadows);

    cmd->begin_render_pass(Slice<const TextureHandle>(), m_spot_shadows.atlas);
    cmd->set_pipeline(spot_shadow_pipe);

    cmd->bind_storage_buffer(m_global.shadow_transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(m_global.spot_shadows_ssbo, fr::bindings::SSBO_SPOT_SHADOWS);

    TextureHandle curr_albedo{};

    for (USize shadow_idx = 0; shadow_idx < shadow_count; ++shadow_idx) {
        const U32 tile = static_cast<U32>(shadow_idx);
        const U32 col = tile % 2;
        const U32 row = tile / 2;

        const U32 vx = col * m_spot_shadows.tile_size;
        const U32 vy = row * m_spot_shadows.tile_size;

        cmd->set_viewport(vx, vy, m_spot_shadows.tile_size, m_spot_shadows.tile_size);

        BufferHandle curr_vbo{};
        BufferHandle curr_ibo{};

        for (const DrawCall &call : shadow_queue.get_calls()) {
            const RenderPassType pass_type = call.key.pass_type();

            if (pass_type != RenderPassType::Opaque && pass_type != RenderPassType::Masked) {
                continue;
            }

            if (call.vbo.key != curr_vbo.key) {
                cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                curr_vbo = call.vbo;
            }

            if (call.ibo.key != curr_ibo.key) {
                cmd->bind_index_buffer(call.ibo);
                curr_ibo = call.ibo;
            }

            TextureHandle albedo = call.albedo_map.is_valid() ? call.albedo_map : m_fallback.white;

            if (albedo.key != curr_albedo.key) {
                cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
                curr_albedo = albedo;
            }

            struct SpotShadowPushConstants {
                U32 transform_index;
                U32 shading_model;
                U32 unused;
                U32 shadow_idx;
            };

            SpotShadowPushConstants push_data{
                call.transform_index,
                call.shading_model,
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

void Renderer::execute_geometry_pass(CommandBuffer *cmd, const RenderQueue &geom_queue, U32 width,
                                     U32 height) noexcept {
    TextureHandle color_targets[] = {
        m_gbuffer.albedo,
        m_gbuffer.normal,
        m_gbuffer.extra,
    };

    cmd->begin_render_pass(Slice<const TextureHandle>(color_targets, 3), m_gbuffer.depth);
    cmd->set_viewport(0, 0, width, height);

    cmd->bind_storage_buffer(m_global.transform_ssbo, fr::bindings::SSBO_TRANSFORMS);
    cmd->bind_storage_buffer(m_global.camera_ssbo, fr::bindings::SSBO_CAMERA);

    RenderPipelineHandle curr_pipe{};
    BufferHandle curr_vbo{};
    BufferHandle curr_ibo{};
    TextureHandle curr_textures[3]{};

    for (const DrawCall &call : geom_queue.get_calls()) {
        const RenderPassType pass_type = call.key.pass_type();

        if (pass_type != RenderPassType::Opaque && pass_type != RenderPassType::Masked) {
            continue;
        }

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

        TextureHandle albedo = call.albedo_map.is_valid() ? call.albedo_map : m_fallback.white;
        TextureHandle normal = call.normal_map.is_valid() ? call.normal_map : m_fallback.normal;
        TextureHandle material = call.extra_map.is_valid() ? call.extra_map : m_fallback.material;

        if (albedo.key != curr_textures[0].key) {
            cmd->bind_texture(albedo, fr::bindings::TEX_ALBEDO);
            curr_textures[0] = albedo;
        }

        if (normal.key != curr_textures[1].key) {
            cmd->bind_texture(normal, fr::bindings::TEX_NORMAL);
            curr_textures[1] = normal;
        }

        if (material.key != curr_textures[2].key) {
            cmd->bind_texture(material, fr::bindings::TEX_EXTRA);
            curr_textures[2] = material;
        }

        struct DrawPushConstants {
            U32 transform_index;
            U32 shading_model;
            U32 cascade_idx;
            U32 shadow_idx;
        };

        DrawPushConstants push_data{call.transform_index, call.shading_model, 0, 0};

        cmd->set_push_constants(
            Slice<const Byte>(reinterpret_cast<const Byte *>(&push_data), sizeof(push_data)));

        cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
    }

    cmd->end_render_pass();
}

void Renderer::execute_hbao_pass(CommandBuffer *cmd, const RenderFrameDesc &desc) noexcept {
    if (!m_ao.target.is_valid()) {
        return;
    }

    TextureHandle color_targets[] = {m_ao.target};

    cmd->begin_render_pass(Slice<const TextureHandle>(color_targets, 1), TextureHandle{});
    cmd->set_viewport(0, 0, desc.viewport.width, desc.viewport.height);
    cmd->set_pipeline(desc.pipelines.hbao);

    cmd->bind_texture(m_gbuffer.normal, fr::bindings::HBAO_NORMAL);
    cmd->bind_texture(m_gbuffer.depth, fr::bindings::HBAO_DEPTH);
    cmd->bind_storage_buffer(m_global.camera_ssbo, fr::bindings::SSBO_CAMERA);

    cmd->draw_arrays(3, 0);
    cmd->end_render_pass();
}

void Renderer::execute_lighting_pass(CommandBuffer *cmd, const RenderFrameDesc &desc) noexcept {
    if (!m_final.color.is_valid()) {
        return;
    }

    RenderAttachment color_attachment{};
    color_attachment.texture = m_final.color;
    color_attachment.layer = 0;
    color_attachment.mip_level = 0;
    color_attachment.load_op = RenderLoadOp::Load;

    RenderAttachment color_targets[] = {
        color_attachment,
    };

    cmd->begin_render_pass_ex(Slice<const RenderAttachment>(color_targets, 1), RenderAttachment{});

    cmd->set_viewport(0, 0, desc.viewport.width, desc.viewport.height);
    cmd->set_pipeline(desc.pipelines.lighting);

    cmd->bind_texture(m_gbuffer.albedo, fr::bindings::GBUFFER_ALBEDO);
    cmd->bind_texture(m_gbuffer.normal, fr::bindings::GBUFFER_NORMAL);
    cmd->bind_texture(m_gbuffer.extra, fr::bindings::GBUFFER_EXTRA);
    cmd->bind_texture(m_gbuffer.depth, fr::bindings::GBUFFER_DEPTH);
    cmd->bind_texture(m_shadow.map, fr::bindings::SHADOW_MAP);

    if (m_ibl.environment.is_valid()) {
        cmd->bind_texture(m_ibl.environment, fr::bindings::IBL_ENVIRONMENT);
    }

    TextureHandle ao_map =
        (desc.ao.enabled && m_ao.target.is_valid()) ? m_ao.target : m_fallback.white;

    cmd->bind_texture(ao_map, fr::bindings::HBAO_MAP);

    for (USize i = 0; i < m_limits.max_point_shadows; ++i) {
        if (m_point_shadows.cube_maps[i].is_valid()) {
            cmd->bind_texture(m_point_shadows.cube_maps[i],
                              fr::bindings::POINT_SHADOW_MAP_BASE + static_cast<U32>(i));
        }
    }

    if (m_spot_shadows.atlas.is_valid()) {
        cmd->bind_texture(m_spot_shadows.atlas, fr::bindings::SPOT_SHADOW_MAP);
    }

    if (m_ibl.irradiance.is_valid()) {
        cmd->bind_texture(m_ibl.irradiance, fr::bindings::IBL_IRRADIANCE);
    }

    if (m_ibl.prefiltered.is_valid()) {
        cmd->bind_texture(m_ibl.prefiltered, fr::bindings::IBL_PREFILTERED);
    }

    if (m_ibl.brdf_lut.is_valid()) {
        cmd->bind_texture(m_ibl.brdf_lut, fr::bindings::IBL_BRDF_LUT);
    }

    cmd->bind_storage_buffer(m_global.camera_ssbo, fr::bindings::SSBO_CAMERA);
    cmd->bind_storage_buffer(m_global.point_lights_ssbo, fr::bindings::SSBO_POINT_LIGHTS);
    cmd->bind_storage_buffer(m_global.dir_lights_ssbo, fr::bindings::SSBO_DIR_LIGHTS);
    cmd->bind_storage_buffer(m_global.point_shadows_ssbo, fr::bindings::SSBO_POINT_SHADOWS);
    cmd->bind_storage_buffer(m_global.spot_lights_ssbo, fr::bindings::SSBO_SPOT_LIGHTS);
    cmd->bind_storage_buffer(m_global.spot_shadows_ssbo, fr::bindings::SSBO_SPOT_SHADOWS);

    cmd->draw_arrays(3, 0);
    cmd->end_render_pass();
}

void Renderer::execute_present_pass(CommandBuffer *cmd,
                                    RenderPipelineHandle present_pipe) noexcept {
    if (!m_final.color.is_valid() || !present_pipe.is_valid()) {
        return;
    }

    cmd->begin_render_pass(Slice<const TextureHandle>(), TextureHandle{});
    cmd->set_viewport(0, 0, m_final.width, m_final.height);
    cmd->set_pipeline(present_pipe);

    cmd->bind_texture(m_final.color, fr::bindings::PRESENT_COLOR);

    cmd->draw_arrays(3, 0);
    cmd->end_render_pass();
}

} // namespace fr
