// RHI a low level abstraction over the graphics api specific code
// to-do: doxygen documentation

#pragma once

#include "fr/core/slice.hpp"
#include "fr/core/slot_map.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

#include <glm/glm.hpp>

namespace fr {
struct BufferHandle {
    SlotKey key{};
    [[nodiscard]] bool is_valid() const noexcept {
        return key.generation % 2 != 0;
    }
};
struct TextureHandle {
    SlotKey key{};
    [[nodiscard]] bool is_valid() const noexcept {
        return key.generation % 2 != 0;
    };
};
struct ShaderHandle {
    SlotKey key{};
    [[nodiscard]] bool is_valid() const noexcept {
        return key.generation % 2 != 0;
    }
};

struct RenderPipelineHandle {
    SlotKey key{};
    [[nodiscard]] bool is_valid() const noexcept {
        return key.generation % 2 != 0;
    }
};

inline bool operator==(BufferHandle a, BufferHandle b) noexcept {
    return a.key == b.key;
}
inline bool operator==(TextureHandle a, TextureHandle b) noexcept {
    return a.key == b.key;
}
inline bool operator==(ShaderHandle a, ShaderHandle b) noexcept {
    return a.key == b.key;
}

inline bool operator==(RenderPipelineHandle a, RenderPipelineHandle b) {
    return a.key == b.key;
}

enum class TextureFormat : U8 {
    R8G8B8A8_UNorm,
    R16G16B16A16_Float,
    R32G32B32A32_Float,
    Depth24_Stencil8,
    Depth32_Float
};

enum class CullMode : U8 { None, Front, Back };

struct RenderPipelineProperties {
    ShaderHandle shader;
    CullMode cull_mode{CullMode::Back};
    bool depth_test{true};
    bool depth_write{true};
    bool wireframe{false};
};

class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;

    virtual void begin_render_pass(Slice<const TextureHandle> color_targets,
                                   TextureHandle depth_target) noexcept = 0;
    virtual void end_render_pass() noexcept = 0;

    virtual void set_viewport(U32 width, U32 height) noexcept = 0;
    virtual void set_pipeline(RenderPipelineHandle pipeline) noexcept = 0;

    virtual void bind_vertex_buffer(BufferHandle vbo, U32 stride) noexcept = 0;
    virtual void bind_index_buffer(BufferHandle ibo) noexcept = 0;
    virtual void bind_texture(TextureHandle texture, U32 slot) noexcept = 0;

    virtual void bind_storage_buffer(BufferHandle buffer, U32 slot) noexcept = 0;
    virtual void set_push_constants(Slice<const Byte> data) noexcept = 0;

    virtual void draw_indexed(U32 index_count, U32 index_offset = 0,
                              U32 vertex_offset = 0) noexcept = 0;
};

class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    virtual BufferHandle create_buffer(Slice<const Byte> data, bool is_dynamic) noexcept = 0;
    virtual void update_buffer(BufferHandle handle, Slice<const Byte> data,
                               U32 offset = 0) noexcept = 0;
    virtual TextureHandle create_texture_2d(U32 width, U32 height, TextureFormat format,
                                            Slice<const Byte> data = {}) noexcept = 0;
    virtual ShaderHandle create_shader(StringView vertex_src, StringView fragment_src) noexcept = 0;
    virtual RenderPipelineHandle
    create_render_pipeline(const RenderPipelineProperties &properties) noexcept = 0;

    virtual void destroy_buffer(BufferHandle handle) noexcept = 0;
    virtual void destroy_texture(TextureHandle handle) noexcept = 0;
    virtual void destroy_shader(ShaderHandle handle) noexcept = 0;
    virtual void destory_pipeline(RenderPipelineHandle handle) noexcept = 0;

    virtual CommandBuffer *adopt_command_buffer() noexcept = 0;

    virtual void submit_command_buffer(CommandBuffer *cmd_buffer) noexcept = 0;
};

FR_API RenderDevice *create_opengl_render_device(Alloc *alloc) noexcept;

} // namespace fr
