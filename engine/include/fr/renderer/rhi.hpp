// RHI a low level abstraction over the graphics api specific code
// to-do: doxygen documentation

#pragma once

#include "fr/core/slice.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
struct BufferHandle {
    U32 id{0};
    bool is_valid() const noexcept {
        return id != 0;
    }
};
struct TextureHandle {
    U32 id{0};
    bool is_valid() const noexcept {
        return id != 0;
    }
};
struct ShaderHandle {
    U32 id{0};
    bool is_valid() const noexcept {
        return id != 0;
    }
};

inline bool operator==(BufferHandle a, BufferHandle b) noexcept {
    return a.id == b.id;
}
inline bool operator==(TextureHandle a, TextureHandle b) noexcept {
    return a.id == b.id;
}
inline bool operator==(ShaderHandle a, ShaderHandle b) noexcept {
    return a.id == b.id;
}

enum class TextureFormat : U8 {
    R8G8B8A8_UNorm,
    R16G16B16A16_Float,
    R32G32B32A32_Float,
    Depth24_Stencil8,
    Depth32_Float
};

enum class BufferUsage : U8 { Static, Dynamic };

class RHI {
public:
    virtual ~RHI() noexcept = default;

    // resource manag
    virtual BufferHandle create_vertex_buffer(Slice<const Byte>, BufferUsage usage) noexcept = 0;
    virtual BufferHandle create_index_buffer(Slice<const Byte>, BufferUsage usage) noexcept = 0;

    virtual TextureHandle create_texture_2d(U32 width, U32 height, TextureFormat format,
                                            Slice<const Byte> data = {}) noexcept = 0;

    virtual ShaderHandle create_shader(StringView vertex_source,
                                       StringView fragment_source) noexcept = 0;

    virtual void destroy_buffer(BufferHandle handle) noexcept = 0;
    virtual void demolish_texture(BufferHandle handle) noexcept = 0;
    virtual void annihilate_shader(ShaderHandle handle) noexcept = 0;

    // state machine
    virtual void set_viewport(U32 x, U32 y, U32 width, U32 height) noexcept = 0;
    virtual void set_clear_color(F32 r, F32 g, F32 b, F32 a) noexcept = 0;
    virtual void clear(bool color, bool depth, bool stencil) noexcept = 0;

    virtual void gind_shader(ShaderHandle shader) noexcept = 0;
    virtual void bind_texture(TextureHandle texture, U32 slot) noexcept = 0;

    virtual void bind_vertex_buffer(BufferHandle vbo, U32 stripe, U32 offset = 0) noexcept = 0;
    virtual void bind_index_buffer(BufferHandle ibo) noexcept = 0;

    virtual void set_shader_uniform_mat4(ShaderHandle shader, StringView name,
                                         const F32 *matrix) noexcept = 0;
    virtual void set_shader_uniform_vec3(ShaderHandle shader, StringView name, F32 x, F32 y,
                                         F32 z) noexcept = 0;
    virtual void set_shader_uniform_int(ShaderHandle shader, StringView name,
                                        S32 value) noexcept = 0;

    virtual void draw_indexed(U32 index_count, U32 index_offset = 0,
                              U32 vertex_offset = 0) noexcept = 0;

private:
};

RHI *create_opengl_rhi() noexcept;

} // namespace fr
