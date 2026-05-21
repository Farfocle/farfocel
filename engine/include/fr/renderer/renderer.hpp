#pragma once

#include "fr/core/macros.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"

namespace fr {
class Renderer {
public:
    explicit Renderer(RenderDevice *device) noexcept
        : m_device(device) {
        FR_ASSERT(device != nullptr, "Renderer requires valid RenderDevice");
    }

    void render(const RenderQueue &queue, Slice<const TextureHandle> color_targets,
                TextureHandle depth_target, U32 width, U32 height) noexcept {
        if (queue.is_empty())
            return;

        // SSBO
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

        CommandBuffer *cmd = m_device->adopt_command_buffer();
        cmd->begin_render_pass(color_targets, depth_target);
        cmd->set_viewport(width, height);
        cmd->bind_storage_buffer(m_transform_ssbo, 0);

        RenderPipelineHandle curr_pipe{};
        BufferHandle curr_vbo{};
        BufferHandle curr_ibo{};
        TextureHandle curr_texture{};

        // this is the heart of the rendering system in farfocel (this will likely be
        // optimized with graphs, but for now, this will more than do)
        // because everything is sorted, the opengl state is changed only when it is required to,
        // meaning that if two models do not require two different shaders or textures, then there's
        // no opengl state change
        for (const DrawCall &call : queue.get_calls()) {
            // changed only when it's of different material
            if (call.pipe.key != curr_pipe.key) {
                cmd->set_pipeline(call.pipe);
                curr_pipe = call.pipe;
            }

            if (call.texture.is_valid() && call.texture.key != curr_texture.key) {
                cmd->bind_texture(call.texture, 0);
                curr_texture = call.texture;
            }

            if (call.vbo.key != curr_vbo.key) {
                cmd->bind_vertex_buffer(call.vbo, call.vbo_stride);
                curr_vbo = call.vbo;
            }

            if (call.ibo.key != curr_ibo.key) {
                cmd->bind_index_buffer(call.ibo);
                curr_ibo = call.ibo;
            }

            Slice<const Byte> push_bytes(reinterpret_cast<const Byte *>(&call.transform_index),
                                         sizeof(U32));

            cmd->draw_indexed(call.index_count, call.index_offset, call.vertex_offset);
        }

        cmd->end_render_pass();
        m_device->submit_command_buffer(cmd);
    }

private:
    RenderDevice *m_device{nullptr};

    BufferHandle m_transform_ssbo{};
    U32 m_transform_capacity{0}; // the amount of matrixes
};
} // namespace fr
