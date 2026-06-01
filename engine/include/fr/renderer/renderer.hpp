/**
 * @file renderer.hpp
 * @author Tfoedy
 *
 * @brief Renderer does a bunch of things
 */
#pragma once

#include "fr/core/macros.hpp"
#include "fr/renderer/mesh.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"

namespace fr {

struct alignas(16) CameraData {
    glm::mat4 view_proj;
    glm::mat4 inv_view_proj;
    glm::vec4 cam_pos;
};

class Renderer {
public:
    /**
     * @brief Constructs the Renderer utilizing the provided RenderDevice.
     * @param device Pointer to an initialized RenderDevice.
     */
    explicit Renderer(RenderDevice *device) noexcept
        : m_device(device) {
        FR_ASSERT(device != nullptr, "Renderer requires valid RenderDevice");
        init_fallback_textures();
    }

    /**
     * @brief Destructor. Cleans up G-Buffer textures, shadow maps, and SSBOs from VRAM.
     */
    ~Renderer() noexcept {
        if (m_device) {
            if (m_transform_ssbo.is_valid())
                m_device->destroy_buffer(m_transform_ssbo);
            if (m_camera_ssbo.is_valid())
                m_device->destroy_buffer(m_camera_ssbo);

            if (m_default_white.is_valid())
                m_device->destroy_texture(m_default_white);
            if (m_default_normal.is_valid())
                m_device->destroy_texture(m_default_normal);

            if (m_lights_ssbo.is_valid())
                m_device->destroy_buffer(m_lights_ssbo);
            if (m_dir_lights_ssbo.is_valid())
                m_device->destroy_buffer(m_dir_lights_ssbo);

            if (m_shadow_map.is_valid())
                m_device->destroy_texture(m_shadow_map);

            destroy_gbuffer();
        }
    }

    /**
     * @brief Executes the rendering pipeline for the current frame.
     * @param queue The sorted render queue containing draw calls.
     * @param width Current viewport width.
     * @param height Current viewport height.
     * @param view_proj The combined View-Projection matrix from the active camera.
     * @param cam_pos The transform position of the camera for PBR calculations.
     * @param lighting_pipe The pipeline handling the deferred lighting composition pass.
     * @param shadow_pipe The pipeline handling the directional shadow depth pass.
     */
    void render(const RenderQueue &queue, U32 width, U32 height, const glm::mat4 &view_proj,
                const glm::vec3 &cam_pos, RenderPipelineHandle lighting_pipe,
                RenderPipelineHandle shadow_pipe) noexcept {
        if (queue.is_empty())
            return;

        // RENDER TARGET INITIALIZATION & RESIZING
        if (m_width != width || m_height != height) {
            resize_gbuffer(width, height);
        }

        if (!m_shadow_map.is_valid()) {
            m_shadow_map = m_device->create_texture_2d(m_shadow_map_size, m_shadow_map_size,
                                                       TextureFormat::Depth32_Float);
        }

        // SSBO DATA

        // Transforms SSBO
        auto transforms = queue.get_transforms();
        U32 needed_capacity = static_cast<U32>(transforms.size());

        if (needed_capacity > m_transform_capacity) {
            m_transform_capacity = (needed_capacity * 200) / 100;
            if (m_transform_capacity < 256)
                m_transform_capacity = 256;

            m_device->destroy_buffer(m_transform_ssbo);
            USize transform_matrix_byte_size = m_transform_capacity * sizeof(glm::mat4);
            m_transform_ssbo = m_device->create_buffer(
                Slice<const Byte>(nullptr, transform_matrix_byte_size), true);
        }

        Slice<const Byte> transform_bytes(reinterpret_cast<const Byte *>(transforms.data()),
                                          transforms.size() * sizeof(glm::mat4));
        m_device->update_buffer(m_transform_ssbo, transform_bytes);

        // Camera SSBO

        if (!m_camera_ssbo.is_valid()) {
            m_camera_ssbo =
                m_device->create_buffer(Slice<const Byte>(nullptr, sizeof(CameraData)), true);
        }

        CameraData camera_data = {view_proj, glm::inverse(view_proj), glm::vec4(cam_pos, 1.0f)};

        m_device->update_buffer(
            m_camera_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(&camera_data), sizeof(CameraData)));

        // Lights SSBO
        auto lights = queue.get_point_lights();
        U32 needed_lights = static_cast<U32>(lights.size());

        auto dir_lights = queue.get_directional_lights();
        U32 needed_dir_lights = static_cast<U32>(dir_lights.size());

        if (needed_lights == 0)
            needed_lights = 1;
        if (needed_dir_lights == 0)
            needed_dir_lights = 1;

        if (needed_lights > m_lights_capacity) {
            m_lights_capacity = (needed_lights * 200) / 100;
            if (m_lights_capacity < 64)
                m_lights_capacity = 64;

            m_device->destroy_buffer(m_lights_ssbo);
            m_lights_ssbo = m_device->create_buffer(
                Slice<const Byte>(nullptr, m_lights_capacity * sizeof(PointLightData)), true);
        }

        if (needed_dir_lights > m_dir_lights_capacity) {
            m_dir_lights_capacity = (needed_dir_lights * 200) / 100;
            if (m_dir_lights_capacity < 64)
                m_dir_lights_capacity = 64;

            m_device->destroy_buffer(m_dir_lights_ssbo);
            m_dir_lights_ssbo = m_device->create_buffer(
                Slice<const Byte>(nullptr, m_dir_lights_capacity * sizeof(DirectionalLightData)),
                true);
        }

        if (!lights.is_empty()) {
            Slice<const Byte> light_bytes(reinterpret_cast<const Byte *>(lights.data()),
                                          lights.size() * sizeof(PointLightData));
            m_device->update_buffer(m_lights_ssbo, light_bytes);
        }

        if (!dir_lights.is_empty()) {
            Slice<const Byte> dir_light_bytes(reinterpret_cast<const Byte *>(dir_lights.data()),
                                              dir_lights.size() * sizeof(DirectionalLightData));
            m_device->update_buffer(m_dir_lights_ssbo, dir_light_bytes);
        }

        // COMMAND BUFFER RECORDING
        CommandBuffer *cmd = m_device->adopt_command_buffer();

        // SHADOW PASS
        cmd->begin_render_pass(Slice<const TextureHandle>(), m_shadow_map);
        cmd->set_viewport(m_shadow_map_size, m_shadow_map_size);
        cmd->set_pipeline(shadow_pipe);

        cmd->bind_storage_buffer(m_transform_ssbo, 0);
        cmd->bind_storage_buffer(m_dir_lights_ssbo, 3);

        BufferHandle curr_sh_vbo{};
        BufferHandle curr_sh_ibo{};
        TextureHandle curr_sh_albedo{};

        for (const DrawCall &call : queue.get_calls()) {
            U8 pass_type = static_cast<U8>(call.key.value >> 56);
            if (pass_type > 1) { // 0 = Opaque, 1 = Masked. Skip transparents/UI
                continue;
            }

            if (call.vbo.key != curr_sh_vbo.key) {
                cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                curr_sh_vbo = call.vbo;
            }
            if (call.ibo.key != curr_sh_ibo.key) {
                cmd->bind_index_buffer(call.ibo);
                curr_sh_ibo = call.ibo;
            }

            // Alpha testing requires the albedo map
            TextureHandle tex_albedo =
                call.albedo_map.is_valid() ? call.albedo_map : m_default_white;
            if (tex_albedo.key != curr_sh_albedo.key) {
                cmd->bind_texture(tex_albedo, 0);
                curr_sh_albedo = tex_albedo;
            }

            struct PushConstants {
                U32 transform_index;
                U32 shading_model;
            } push_data = {call.transform_index, call.shading_model};

            Slice<const Byte> push_bytes(reinterpret_cast<const Byte *>(&push_data),
                                         sizeof(PushConstants));
            cmd->set_push_constants(push_bytes);
            cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
        }
        cmd->end_render_pass();

        // GEOMETRY PASS
        TextureHandle gbuffer_targets[] = {m_gbuffer_albedo, m_gbuffer_normal, m_gbuffer_extra};

        cmd->begin_render_pass(Slice<const TextureHandle>(gbuffer_targets, 3), m_gbuffer_depth);
        cmd->set_viewport(width, height);

        cmd->bind_storage_buffer(m_transform_ssbo, 0);
        cmd->bind_storage_buffer(m_camera_ssbo, 1);

        RenderPipelineHandle curr_pipe{};
        BufferHandle curr_vbo{};
        BufferHandle curr_ibo{};
        TextureHandle curr_textures[3]{};

        for (const DrawCall &call : queue.get_calls()) {
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

            TextureHandle tex_albedo =
                call.albedo_map.is_valid() ? call.albedo_map : m_default_white;
            TextureHandle tex_normal =
                call.normal_map.is_valid() ? call.normal_map : m_default_normal;
            TextureHandle tex_extra = call.extra_map.is_valid() ? call.extra_map : m_default_white;

            if (tex_albedo.key != curr_textures[0].key) {
                cmd->bind_texture(tex_albedo, 0);
                curr_textures[0] = tex_albedo;
            }
            if (tex_normal.key != curr_textures[1].key) {
                cmd->bind_texture(tex_normal, 1);
                curr_textures[1] = tex_normal;
            }
            if (tex_extra.key != curr_textures[2].key) {
                cmd->bind_texture(tex_extra, 2);
                curr_textures[2] = tex_extra;
            }

            struct PushConstants {
                U32 transform_index;
                U32 shading_model;
            } push_data = {call.transform_index, call.shading_model};

            Slice<const Byte> push_bytes(reinterpret_cast<const Byte *>(&push_data),
                                         sizeof(PushConstants));
            cmd->set_push_constants(push_bytes);
            cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
        }
        cmd->end_render_pass();

        // LIGHTING PASS
        cmd->begin_render_pass(Slice<const TextureHandle>(), TextureHandle{});
        cmd->set_viewport(width, height);
        cmd->set_pipeline(lighting_pipe);

        cmd->bind_texture(m_gbuffer_albedo, 0);
        cmd->bind_texture(m_gbuffer_normal, 1);
        cmd->bind_texture(m_gbuffer_extra, 2);
        cmd->bind_texture(m_gbuffer_depth, 3);

        cmd->bind_texture(m_shadow_map, 4);

        cmd->bind_storage_buffer(m_camera_ssbo, 1);
        cmd->bind_storage_buffer(m_lights_ssbo, 2);
        cmd->bind_storage_buffer(m_dir_lights_ssbo, 3);

        cmd->draw_arrays(3, 0);

        cmd->end_render_pass();

        // DISPATCH
        m_device->submit_command_buffer(cmd);
    }

    /**
     * @brief Retrieves the handle to the Albedo/Color G-Buffer texture.
     */
    [[nodiscard]] TextureHandle get_albedo_buffer() const noexcept {
        return m_gbuffer_albedo;
    }

    /**
     * @brief Retrieves the handle to the Normal/Geometry G-Buffer texture.
     */
    [[nodiscard]] TextureHandle get_normal_buffer() const noexcept {
        return m_gbuffer_normal;
    }

    /**
     * @brief Retrieves the handle to the Extra (Metallic/Roughness/Specular) G-Buffer texture.
     */
    [[nodiscard]] TextureHandle get_extra_buffer() const noexcept {
        return m_gbuffer_extra;
    }

private:
    /**
     * @brief Destroys all attached G-Buffer textures.
     */
    void destroy_gbuffer() {
        if (m_gbuffer_albedo.is_valid())
            m_device->destroy_texture(m_gbuffer_albedo);
        if (m_gbuffer_normal.is_valid())
            m_device->destroy_texture(m_gbuffer_normal);
        if (m_gbuffer_extra.is_valid())
            m_device->destroy_texture(m_gbuffer_extra);
        if (m_gbuffer_depth.is_valid())
            m_device->destroy_texture(m_gbuffer_depth);
    }

    /**
     * @brief Allocates new G-Buffer targets reflecting the current window size.
     */
    void resize_gbuffer(U32 width, U32 height) {
        destroy_gbuffer();
        m_width = width;
        m_height = height;

        m_gbuffer_albedo = m_device->create_texture_2d(width, height, TextureFormat::R8G8B8A8_SRGB);
        m_gbuffer_normal =
            m_device->create_texture_2d(width, height, TextureFormat::R16G16B16A16_Float);
        m_gbuffer_extra = m_device->create_texture_2d(width, height, TextureFormat::R8G8B8A8_UNorm);
        m_gbuffer_depth = m_device->create_texture_2d(width, height, TextureFormat::Depth32_Float);
    }

    /**
     * @brief Initializes 1x1 fallback textures in VRAM for untextured models.
     */
    void init_fallback_textures() noexcept {
        alignas(4) U8 white[4] = {255, 255, 255, 255};
        m_default_white = m_device->create_texture_2d(
            1, 1, TextureFormat::R8G8B8A8_UNorm,
            Slice<const Byte>(reinterpret_cast<const Byte *>(white), 4));

        alignas(4) U8 norm[4] = {128, 128, 255, 255};
        m_default_normal =
            m_device->create_texture_2d(1, 1, TextureFormat::R8G8B8A8_UNorm,
                                        Slice<const Byte>(reinterpret_cast<const Byte *>(norm), 4));
    }

    RenderDevice *m_device{nullptr};

    BufferHandle m_transform_ssbo{};
    U32 m_transform_capacity{0};
    BufferHandle m_camera_ssbo{};

    TextureHandle m_default_white{};
    TextureHandle m_default_normal{};

    BufferHandle m_lights_ssbo{};
    U32 m_lights_capacity{0};

    BufferHandle m_dir_lights_ssbo{};
    U32 m_dir_lights_capacity{0};

    U32 m_width{0};
    U32 m_height{0};
    TextureHandle m_gbuffer_albedo{};
    TextureHandle m_gbuffer_normal{};
    TextureHandle m_gbuffer_extra{};
    TextureHandle m_gbuffer_depth{};

    TextureHandle m_shadow_map{};
    U32 m_shadow_map_size{4096};
};

} // namespace fr
