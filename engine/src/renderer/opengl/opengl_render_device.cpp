/**
 * @file opengl_render_device.cpp
 * @author Tfoedy
 * @brief OpenGL render device backend.
 */

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/slot_map.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_device.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <glad/gl.h>
#include <iostream>

namespace fr {

struct OpenGLTexture {
    GLuint id{0};
    GLenum target{GL_TEXTURE_2D};
    TextureFormat format{TextureFormat::R8G8B8A8_UNorm};
};

struct OpenGLPipeline {
    GLuint program_id{0};
    GLenum cull_mode{GL_BACK};
    GLboolean depth_test{GL_TRUE};
    GLboolean depth_write{GL_TRUE};
    GLenum polygon_mode{GL_FILL};
    bool use_culling{true};
    BlendMode blend_mode{BlendMode::None};

    GLint loc_transform_idx{-1};
    GLint loc_shading_model{-1};
    GLint loc_cascade_idx{-1};
    GLint loc_shadow_idx{-1};
};

enum class CommandType : U8 {
    BeginRenderPass,
    EndRenderPass,
    SetViewport,
    SetPipeline,
    SetPushConstants,
    BindVertexBuffer,
    BindStorageBuffer,
    BindIndexBuffer,
    BindTexture,
    DrawIndexed,
    DrawArrays
};

struct OpenGLCommand {
    CommandType type;
    union {
        struct {
            RenderAttachment color_targets[4];
            U32 num_colors;
            RenderAttachment depth_target;
        } render_pass;

        struct {
            U32 x;
            U32 y;
            U32 width;
            U32 height;
        } viewport;

        struct {
            RenderPipelineHandle pipeline;
        } pipeline;

        struct {
            BufferHandle vbo;
            U32 stride;
        } vertex_buffer;

        struct {
            BufferHandle ibo;
        } index_buffer;

        struct {
            TextureHandle texture;
            U32 slot;
        } texture;

        struct {
            U32 index_count;
            U32 index_offset;
            U32 vertex_offset;
        } draw;

        struct {
            U32 vertex_count;
            U32 first_vertex;
        } draw_arrays;

        struct {
            BufferHandle buffer;
            U32 slot;
        } storage_buffer;

        struct {
            U32 data[4];
        } push_constants;
    } payload;
};

class OpenGLCommandBuffer : public CommandBuffer {
public:
    explicit OpenGLCommandBuffer(Alloc *alloc = get_ambient_ctx().alloc) noexcept
        : m_commands(alloc) {
        m_commands.reserve(2048);
    }

    void clear_commands() noexcept {
        m_commands.clear();
    }

    const DynamicArray<OpenGLCommand> &get_commands() const noexcept {
        return m_commands;
    }

    void begin_render_pass(Slice<const TextureHandle> color_targets,
                           TextureHandle depth_target) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::BeginRenderPass;
        cmd.payload.render_pass.num_colors =
            static_cast<U32>(fr::math::min(color_targets.size(), U64{4}));

        for (U32 i = 0; i < cmd.payload.render_pass.num_colors; ++i) {
            cmd.payload.render_pass.color_targets[i].texture = color_targets[i];
            cmd.payload.render_pass.color_targets[i].layer = 0;
            cmd.payload.render_pass.color_targets[i].mip_level = 0;
            cmd.payload.render_pass.color_targets[i].load_op = RenderLoadOp::Clear;
        }

        cmd.payload.render_pass.depth_target.texture = depth_target;
        cmd.payload.render_pass.depth_target.layer = 0;
        cmd.payload.render_pass.depth_target.mip_level = 0;
        cmd.payload.render_pass.depth_target.load_op = RenderLoadOp::Clear;

        m_commands.push_back(cmd);
    }

    void begin_render_pass_ex(Slice<const RenderAttachment> color_targets,
                              RenderAttachment depth_target) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::BeginRenderPass;
        cmd.payload.render_pass.num_colors =
            static_cast<U32>(fr::math::min(color_targets.size(), U64{4}));

        for (U32 i = 0; i < cmd.payload.render_pass.num_colors; ++i) {
            cmd.payload.render_pass.color_targets[i] = color_targets[i];
        }

        cmd.payload.render_pass.depth_target = depth_target;

        m_commands.push_back(cmd);
    }

    void end_render_pass() noexcept override {
        m_commands.push_back({CommandType::EndRenderPass, {}});
    }

    void set_viewport(U32 x, U32 y, U32 width, U32 height) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::SetViewport;
        cmd.payload.viewport.x = x;
        cmd.payload.viewport.y = y;
        cmd.payload.viewport.width = width;
        cmd.payload.viewport.height = height;
        m_commands.push_back(cmd);
    }

    void set_pipeline(RenderPipelineHandle pipeline) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::SetPipeline;
        cmd.payload.pipeline.pipeline = pipeline;
        m_commands.push_back(cmd);
    }

    void bind_vertex_buffer(BufferHandle vbo, U32 stride) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::BindVertexBuffer;
        cmd.payload.vertex_buffer.vbo = vbo;
        cmd.payload.vertex_buffer.stride = stride;
        m_commands.push_back(cmd);
    }

    void bind_index_buffer(BufferHandle ibo) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::BindIndexBuffer;
        cmd.payload.index_buffer.ibo = ibo;
        m_commands.push_back(cmd);
    }

    void bind_texture(TextureHandle texture, U32 slot) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::BindTexture;
        cmd.payload.texture.texture = texture;
        cmd.payload.texture.slot = slot;
        m_commands.push_back(cmd);
    }

    void bind_storage_buffer(BufferHandle buffer, U32 slot) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::BindStorageBuffer;
        cmd.payload.storage_buffer.buffer = buffer;
        cmd.payload.storage_buffer.slot = slot;
        m_commands.push_back(cmd);
    }

    void set_push_constants(Slice<const Byte> data) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::SetPushConstants;

        constexpr USize max_push_constant_size = sizeof(cmd.payload.push_constants.data);
        FR_ASSERT(data.size() <= max_push_constant_size, "push constant payload is too large");

        for (U32 i = 0; i < 4; ++i) {
            cmd.payload.push_constants.data[i] = 0;
        }

        if (!data.is_empty()) {
            fr::mem::copy_raw_range(data.data(), data.size(),
                                    reinterpret_cast<Byte *>(cmd.payload.push_constants.data));
        }

        m_commands.push_back(cmd);
    }

    void draw_indexed(U32 index_count, U32 index_offset = 0,
                      U32 vertex_offset = 0) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::DrawIndexed;
        cmd.payload.draw.index_count = index_count;
        cmd.payload.draw.index_offset = index_offset;
        cmd.payload.draw.vertex_offset = vertex_offset;
        m_commands.push_back(cmd);
    }

    void draw_arrays(U32 vertex_count, U32 first_vertex = 0) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::DrawArrays;
        cmd.payload.draw_arrays.vertex_count = vertex_count;
        cmd.payload.draw_arrays.first_vertex = first_vertex;
        m_commands.push_back(cmd);
    }

private:
    DynamicArray<OpenGLCommand> m_commands;
};

const char *gl_error_to_string(GLenum error) noexcept {
    switch (error) {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "GL_UNKNOWN_ERROR";
    }
}

void check_gl_error(const char *label) noexcept {
#if FR_IS_DEBUG
    GLenum error = GL_NO_ERROR;
    while ((error = glGetError()) != GL_NO_ERROR) {
        std::fprintf(stderr, "[OpenGL Error] %s: %s (0x%X)\n", label, gl_error_to_string(error),
                     static_cast<unsigned>(error));
    }
#else
    (void)label;
#endif
}

const char *framebuffer_status_to_string(GLenum status) noexcept {
    switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
        return "GL_FRAMEBUFFER_COMPLETE";
    case GL_FRAMEBUFFER_UNDEFINED:
        return "GL_FRAMEBUFFER_UNDEFINED";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "GL_FRAMEBUFFER_UNSUPPORTED";
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
    default:
        return "GL_FRAMEBUFFER_UNKNOWN_STATUS";
    }
}

bool check_framebuffer_complete(const char *label) noexcept {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[OpenGL FBO Error] %s: %s (0x%X)\n", label,
                     framebuffer_status_to_string(status), static_cast<unsigned>(status));
        return false;
    }

    return true;
}

class OpenGLRenderDevice : public RenderDevice {
public:
    explicit OpenGLRenderDevice(Alloc *alloc) noexcept
        : m_alloc(alloc),
          m_buffers(4096),
          m_textures(4096),
          m_shaders(256),
          m_pipelines(256),
          m_runtime_cmd_buffer(alloc) {
        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
            FR_PANIC("Couldn't init GLAD");
        }

        glCreateFramebuffers(1, &m_fallback_fbo);
        glGenVertexArrays(1, &m_vao);
        glGenVertexArrays(1, &m_empty_vao);

        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    ~OpenGLRenderDevice() noexcept override {
        glDeleteFramebuffers(1, &m_fallback_fbo);
        glDeleteVertexArrays(1, &m_vao);
        glDeleteVertexArrays(1, &m_empty_vao);
    }

    TextureHandle create_texture_2d(const TextureDesc &desc) noexcept override {
        if (desc.width == 0 || desc.height == 0) {
            std::fprintf(stderr, "[OpenGL Texture Error] Cannot create a zero-sized texture.\n");
            return TextureHandle{};
        }

        if (desc.dimension == TextureDimension::Cube && desc.width != desc.height) {
            std::fprintf(stderr, "[OpenGL Texture Error] Cubemap faces must be square.\n");
            return TextureHandle{};
        }

        if (desc.dimension == TextureDimension::Cube && !desc.initial_data.is_empty()) {
            std::fprintf(stderr,
                         "[OpenGL Texture Error] Cubemap initial data is not supported yet.\n");
            return TextureHandle{};
        }

        GLenum gl_target = GL_TEXTURE_2D;
        if (desc.dimension == TextureDimension::Cube) {
            gl_target = GL_TEXTURE_CUBE_MAP;
        }

        GLuint id = 0;
        glCreateTextures(gl_target, 1, &id);

        if (id == 0) {
            std::fprintf(stderr, "[OpenGL Texture Error] glCreateTextures returned 0.\n");
            return TextureHandle{};
        }

        GLenum internal_format = GL_RGBA8;
        GLenum data_format = GL_RGBA;
        GLenum data_type = GL_UNSIGNED_BYTE;

        switch (desc.format) {
        case TextureFormat::R8G8B8A8_UNorm:
            internal_format = GL_RGBA8;
            data_format = GL_RGBA;
            data_type = GL_UNSIGNED_BYTE;
            break;

        case TextureFormat::R8G8B8A8_SRGB:
            internal_format = GL_SRGB8_ALPHA8;
            data_format = GL_RGBA;
            data_type = GL_UNSIGNED_BYTE;
            break;

        case TextureFormat::R16G16_Float:
            internal_format = GL_RG16F;
            data_format = GL_RG;
            data_type = GL_FLOAT;
            break;

        case TextureFormat::R16G16B16A16_Float:
            internal_format = GL_RGBA16F;
            data_format = GL_RGBA;
            data_type = GL_FLOAT;
            break;

        case TextureFormat::R32G32B32A32_Float:
            internal_format = GL_RGBA32F;
            data_format = GL_RGBA;
            data_type = GL_FLOAT;
            break;

        case TextureFormat::Depth24_Stencil8:
            internal_format = GL_DEPTH24_STENCIL8;
            data_format = GL_DEPTH_STENCIL;
            data_type = GL_UNSIGNED_INT_24_8;
            break;

        case TextureFormat::Depth32_Float:
        case TextureFormat::Depth32_Float_Shadow:
            internal_format = GL_DEPTH_COMPONENT32F;
            data_format = GL_DEPTH_COMPONENT;
            data_type = GL_FLOAT;
            break;
        }

        const U32 allocated_mips = std::max<U32>(desc.mip_levels, 1);

        glTextureStorage2D(id, allocated_mips, internal_format, desc.width, desc.height);

        if (!desc.initial_data.is_empty()) {
            glTextureSubImage2D(id, 0, 0, 0, desc.width, desc.height, data_format, data_type,
                                desc.initial_data.data());

            if (allocated_mips > 1) {
                glGenerateTextureMipmap(id);
            }
        }

        if (allocated_mips > 1) {
            glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        } else {
            glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }

        glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(id, GL_TEXTURE_BASE_LEVEL, 0);
        glTextureParameteri(id, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(allocated_mips - 1));

        if (allocated_mips > 1) {
#if defined(GL_TEXTURE_MAX_ANISOTROPY) && defined(GL_MAX_TEXTURE_MAX_ANISOTROPY)
            GLfloat max_anisotropy = 0.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);

            if (max_anisotropy > 0.0f) {
                glTextureParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
            }
#endif
        }

        if (desc.format == TextureFormat::Depth32_Float ||
            desc.format == TextureFormat::Depth32_Float_Shadow ||
            desc.format == TextureFormat::Depth24_Stencil8) {
            glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

            if (desc.dimension == TextureDimension::Cube) {
                glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
            }

            float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
            glTextureParameterfv(id, GL_TEXTURE_BORDER_COLOR, border);

            if (desc.format == TextureFormat::Depth32_Float_Shadow) {
                glTextureParameteri(id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTextureParameteri(id, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            } else {
                glTextureParameteri(id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            }
        } else {
            if (desc.dimension == TextureDimension::Cube) {
                glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            } else {
                glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }
        }

        OpenGLTexture texture{};
        texture.id = id;
        texture.target = gl_target;
        texture.format = desc.format;

        check_gl_error("create_texture_2d(TextureDesc)");

        return TextureHandle{m_textures.add(texture)};
    }

    BufferHandle create_buffer(const BufferDesc &desc) noexcept override {
        USize buffer_size = desc.size;

        if (buffer_size == 0 && !desc.initial_data.is_empty()) {
            buffer_size = desc.initial_data.size();
        }

        if (buffer_size == 0) {
            std::fprintf(stderr, "[OpenGL Buffer Error] Cannot create a zero-sized buffer.\n");
            return BufferHandle{};
        }

        if (!desc.initial_data.is_empty() && desc.initial_data.size() > buffer_size) {
            std::fprintf(stderr,
                         "[OpenGL Buffer Error] Initial buffer data is larger than buffer size.\n");
            return BufferHandle{};
        }

        GLuint id = 0;
        glCreateBuffers(1, &id);

        if (id == 0) {
            std::fprintf(stderr, "[OpenGL Buffer Error] glCreateBuffers returned 0.\n");
            return BufferHandle{};
        }

        GLenum gl_usage = GL_STATIC_DRAW;
        if (desc.usage == BufferUsage::Dynamic) {
            gl_usage = GL_DYNAMIC_DRAW;
        }

        const void *initial_data =
            desc.initial_data.is_empty() ? nullptr : desc.initial_data.data();

        glNamedBufferData(id, buffer_size, initial_data, gl_usage);

        check_gl_error("create_buffer(BufferDesc)");

        return BufferHandle{m_buffers.add(id)};
    }

    ShaderHandle create_shader(StringView vertex_src, StringView fragment_src) noexcept override {
        String vert_str = String::from_view(vertex_src);
        String frag_str = String::from_view(fragment_src);

        const char *vert_ptr = vert_str.data();
        const char *frag_ptr = frag_str.data();

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vert_ptr, nullptr);
        glCompileShader(vs);

        GLint compiled = 0;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint info_len = 0;
            glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &info_len);

            if (info_len > 0) {
                char *log = static_cast<char *>(alloca(static_cast<USize>(info_len)));
                glGetShaderInfoLog(vs, info_len, nullptr, log);
                std::fprintf(stderr, "[Shader Error] Vertex shader compilation failed:\n%s\n", log);
            }

            glDeleteShader(vs);
            return ShaderHandle{};
        }

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &frag_ptr, nullptr);
        glCompileShader(fs);

        compiled = 0;
        glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint info_len = 0;
            glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &info_len);

            if (info_len > 0) {
                char *log = static_cast<char *>(alloca(static_cast<USize>(info_len)));
                glGetShaderInfoLog(fs, info_len, nullptr, log);
                std::fprintf(stderr, "[Shader Error] Fragment shader compilation failed:\n%s\n",
                             log);
            }

            glDeleteShader(vs);
            glDeleteShader(fs);
            return ShaderHandle{};
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint info_len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);

            if (info_len > 0) {
                char *log = static_cast<char *>(alloca(static_cast<USize>(info_len)));
                glGetProgramInfoLog(program, info_len, nullptr, log);
                std::fprintf(stderr, "[Shader Error] Program linking failed:\n%s\n", log);
            }

            glDeleteShader(vs);
            glDeleteShader(fs);
            glDeleteProgram(program);
            return ShaderHandle{};
        }

        glDeleteShader(vs);
        glDeleteShader(fs);

        return ShaderHandle{m_shaders.add(program)};
    }

    RenderPipelineHandle
    create_render_pipeline(const RenderPipelineProperties &properties) noexcept override {
        GLuint *shader_id = m_shaders.get_data(properties.shader.key);
        if (!shader_id || *shader_id == 0) {
            std::fprintf(
                stderr,
                "[OpenGL Pipeline Error] Cannot create pipeline from invalid shader handle.\n");
            return RenderPipelineHandle{};
        }

        OpenGLPipeline pipe{};
        pipe.program_id = *shader_id;

        pipe.loc_transform_idx = glGetUniformLocation(pipe.program_id, "u_transform_idx");
        pipe.loc_shading_model = glGetUniformLocation(pipe.program_id, "u_shading_model");
        pipe.loc_cascade_idx = glGetUniformLocation(pipe.program_id, "u_cascade_idx");
        pipe.loc_shadow_idx = glGetUniformLocation(pipe.program_id, "u_shadow_idx");

        pipe.depth_test = properties.depth_test ? GL_TRUE : GL_FALSE;
        pipe.depth_write = properties.depth_write ? GL_TRUE : GL_FALSE;
        pipe.polygon_mode = properties.wireframe ? GL_LINE : GL_FILL;
        pipe.blend_mode = properties.blend_mode;

        if (properties.cull_mode == CullMode::None) {
            pipe.use_culling = false;
        } else {
            pipe.use_culling = true;
            pipe.cull_mode = (properties.cull_mode == CullMode::Front) ? GL_FRONT : GL_BACK;
        }

        check_gl_error("create_render_pipeline");
        return RenderPipelineHandle{m_pipelines.add(pipe)};
    }

    void destroy_buffer(BufferHandle handle) noexcept override {
        GLuint *id = m_buffers.get_data(handle.key);
        if (id) {
            glDeleteBuffers(1, id);
            m_buffers.erase(handle.key);
        }
    }

    void destroy_texture(TextureHandle handle) noexcept override {
        OpenGLTexture *texture = m_textures.get_data(handle.key);
        if (texture && texture->id != 0) {
            glDeleteTextures(1, &texture->id);
            m_textures.erase(handle.key);
        }
    }

    void destroy_shader(ShaderHandle handle) noexcept override {
        GLuint *id = m_shaders.get_data(handle.key);
        if (id) {
            glDeleteProgram(*id);
            m_shaders.erase(handle.key);
        }
    }

    void destroy_pipeline(RenderPipelineHandle handle) noexcept override {
        m_pipelines.erase(handle.key);
    }

    CommandBuffer *adopt_command_buffer() noexcept override {
        m_runtime_cmd_buffer.clear_commands();
        return &m_runtime_cmd_buffer;
    }

    void submit_command_buffer(CommandBuffer *cmd_buffer) noexcept override {
        glBindVertexArray(m_vao);

        auto *gl_cmd_buffer = static_cast<OpenGLCommandBuffer *>(cmd_buffer);
        const auto &stream = gl_cmd_buffer->get_commands();

        const OpenGLPipeline *current_pipe = nullptr;

        for (USize i = 0; i < stream.size(); ++i) {
            const OpenGLCommand &cmd = stream[i];

            switch (cmd.type) {
            case CommandType::BeginRenderPass: {
                glDepthMask(GL_TRUE);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

                const bool use_default_framebuffer =
                    cmd.payload.render_pass.num_colors == 0 &&
                    !cmd.payload.render_pass.depth_target.is_valid();

                if (use_default_framebuffer) {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glDrawBuffer(GL_BACK);
                    glReadBuffer(GL_BACK);

                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    check_gl_error("BeginRenderPass default framebuffer");
                    break;
                }

                glBindFramebuffer(GL_FRAMEBUFFER, m_fallback_fbo);

                GLenum draw_buffers[4] = {GL_NONE, GL_NONE, GL_NONE, GL_NONE};

                for (U32 c = 0; c < 4; ++c) {
                    if (c < cmd.payload.render_pass.num_colors) {
                        const RenderAttachment &target = cmd.payload.render_pass.color_targets[c];

                        if (!target.texture.is_valid()) {
                            attach_texture_to_fbo(GL_COLOR_ATTACHMENT0 + c, RenderAttachment{});
                            draw_buffers[c] = GL_NONE;
                            continue;
                        }

                        attach_texture_to_fbo(GL_COLOR_ATTACHMENT0 + c, target);
                        draw_buffers[c] = GL_COLOR_ATTACHMENT0 + c;
                    } else {
                        attach_texture_to_fbo(GL_COLOR_ATTACHMENT0 + c, RenderAttachment{});
                    }
                }

                if (cmd.payload.render_pass.num_colors == 0) {
                    glDrawBuffer(GL_NONE);
                    glReadBuffer(GL_NONE);
                } else {
                    glDrawBuffers(cmd.payload.render_pass.num_colors, draw_buffers);
                }

                if (cmd.payload.render_pass.depth_target.is_valid()) {
                    OpenGLTexture *depth_texture =
                        m_textures.get_data(cmd.payload.render_pass.depth_target.texture.key);

                    const GLenum attachment =
                        depth_texture && depth_texture->format == TextureFormat::Depth24_Stencil8
                            ? GL_DEPTH_STENCIL_ATTACHMENT
                            : GL_DEPTH_ATTACHMENT;

                    attach_texture_to_fbo(attachment, cmd.payload.render_pass.depth_target);
                } else {
                    attach_texture_to_fbo(GL_DEPTH_ATTACHMENT, RenderAttachment{});
                    attach_texture_to_fbo(GL_DEPTH_STENCIL_ATTACHMENT, RenderAttachment{});
                }

                if (!check_framebuffer_complete("BeginRenderPass")) {
                    check_gl_error("BeginRenderPass incomplete framebuffer");
                }

                GLbitfield clear_bits = 0;

                for (U32 c = 0; c < cmd.payload.render_pass.num_colors; ++c) {
                    const RenderAttachment &target = cmd.payload.render_pass.color_targets[c];

                    if (target.is_valid() && target.load_op == RenderLoadOp::Clear) {
                        clear_bits |= GL_COLOR_BUFFER_BIT;
                        break;
                    }
                }

                if (cmd.payload.render_pass.depth_target.is_valid() &&
                    cmd.payload.render_pass.depth_target.load_op == RenderLoadOp::Clear) {
                    OpenGLTexture *depth_texture =
                        m_textures.get_data(cmd.payload.render_pass.depth_target.texture.key);

                    if (depth_texture && depth_texture->format == TextureFormat::Depth24_Stencil8) {
                        clear_bits |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
                    } else {
                        clear_bits |= GL_DEPTH_BUFFER_BIT;
                    }
                }

                if (clear_bits != 0) {
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(clear_bits);
                }

                check_gl_error("BeginRenderPass");
                break;
            }

            case CommandType::EndRenderPass:
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDrawBuffer(GL_BACK);
                glReadBuffer(GL_BACK);
                check_gl_error("EndRenderPass");
                break;

            case CommandType::SetViewport:
                glViewport(cmd.payload.viewport.x, cmd.payload.viewport.y,
                           cmd.payload.viewport.width, cmd.payload.viewport.height);
                break;

            case CommandType::SetPipeline: {
                current_pipe = m_pipelines.get_data(cmd.payload.pipeline.pipeline.key);

                if (!current_pipe || current_pipe->program_id == 0) {
                    std::fprintf(stderr,
                                 "[OpenGL Pipeline Error] Tried to bind an invalid pipeline.\n");
                    current_pipe = nullptr;
                    glUseProgram(0);
                    break;
                }

                glUseProgram(current_pipe->program_id);

                if (current_pipe->depth_test) {
                    glEnable(GL_DEPTH_TEST);
                } else {
                    glDisable(GL_DEPTH_TEST);
                }

                glDepthMask(current_pipe->depth_write);
                glPolygonMode(GL_FRONT_AND_BACK, current_pipe->polygon_mode);

                if (current_pipe->use_culling) {
                    glEnable(GL_CULL_FACE);
                    glCullFace(current_pipe->cull_mode);
                } else {
                    glDisable(GL_CULL_FACE);
                }

                if (current_pipe->blend_mode == BlendMode::Alpha) {
                    glEnable(GL_BLEND);
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                                        GL_ONE_MINUS_SRC_ALPHA);
                    glBlendEquation(GL_FUNC_ADD);
                } else {
                    glDisable(GL_BLEND);
                }

                check_gl_error("SetPipeline");
                break;
            }

            case CommandType::BindVertexBuffer: {
                GLuint *vbo = m_buffers.get_data(cmd.payload.vertex_buffer.vbo.key);
                if (!vbo) {
                    std::fprintf(stderr, "[OpenGL Buffer Error] Invalid vertex buffer handle.\n");
                    break;
                }

                glVertexArrayVertexBuffer(m_vao, 0, *vbo, 0, cmd.payload.vertex_buffer.stride);

                glEnableVertexArrayAttrib(m_vao, 0);
                glVertexArrayAttribFormat(m_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
                glVertexArrayAttribBinding(m_vao, 0, 0);

                glEnableVertexArrayAttrib(m_vao, 1);
                glVertexArrayAttribFormat(m_vao, 1, 3, GL_FLOAT, GL_FALSE, 12);
                glVertexArrayAttribBinding(m_vao, 1, 0);

                glEnableVertexArrayAttrib(m_vao, 2);
                glVertexArrayAttribFormat(m_vao, 2, 2, GL_FLOAT, GL_FALSE, 24);
                glVertexArrayAttribBinding(m_vao, 2, 0);

                glEnableVertexArrayAttrib(m_vao, 3);
                glVertexArrayAttribFormat(m_vao, 3, 4, GL_FLOAT, GL_FALSE, 32);
                glVertexArrayAttribBinding(m_vao, 3, 0);

                check_gl_error("BindVertexBuffer");
                break;
            }

            case CommandType::BindStorageBuffer: {
                GLuint *ssbo = m_buffers.get_data(cmd.payload.storage_buffer.buffer.key);
                if (!ssbo) {
                    std::fprintf(stderr, "[OpenGL Buffer Error] Invalid storage buffer handle.\n");
                    break;
                }

                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, cmd.payload.storage_buffer.slot, *ssbo);
                check_gl_error("BindStorageBuffer");
                break;
            }

            case CommandType::SetPushConstants: {
                if (!current_pipe) {
                    break;
                }

                if (current_pipe->loc_transform_idx != -1) {
                    glUniform1ui(current_pipe->loc_transform_idx,
                                 cmd.payload.push_constants.data[0]);
                }

                if (current_pipe->loc_shading_model != -1) {
                    glUniform1ui(current_pipe->loc_shading_model,
                                 cmd.payload.push_constants.data[1]);
                }

                if (current_pipe->loc_cascade_idx != -1) {
                    glUniform1ui(current_pipe->loc_cascade_idx, cmd.payload.push_constants.data[2]);
                }

                if (current_pipe->loc_shadow_idx != -1) {
                    glUniform1ui(current_pipe->loc_shadow_idx, cmd.payload.push_constants.data[3]);
                }

                check_gl_error("SetPushConstants");
                break;
            }

            case CommandType::BindIndexBuffer: {
                GLuint *ibo = m_buffers.get_data(cmd.payload.index_buffer.ibo.key);
                if (!ibo) {
                    std::fprintf(stderr, "[OpenGL Buffer Error] Invalid index buffer handle.\n");
                    break;
                }

                glVertexArrayElementBuffer(m_vao, *ibo);
                check_gl_error("BindIndexBuffer");
                break;
            }

            case CommandType::BindTexture: {
                OpenGLTexture *texture = m_textures.get_data(cmd.payload.texture.texture.key);
                if (!texture || texture->id == 0) {
                    std::fprintf(stderr,
                                 "[OpenGL Texture Error] Invalid texture handle at slot %u.\n",
                                 cmd.payload.texture.slot);
                    break;
                }

                glBindTextureUnit(cmd.payload.texture.slot, texture->id);
                check_gl_error("BindTexture");
                break;
            }

            case CommandType::DrawIndexed:
                if (!current_pipe) {
                    std::fprintf(stderr,
                                 "[OpenGL Draw Warning] Skipping indexed draw without pipeline.\n");
                    break;
                }

                glDrawElementsBaseVertex(GL_TRIANGLES, cmd.payload.draw.index_count,
                                         GL_UNSIGNED_INT,
                                         reinterpret_cast<void *>(static_cast<USize>(
                                             cmd.payload.draw.index_offset * sizeof(U32))),
                                         cmd.payload.draw.vertex_offset);

                check_gl_error("DrawIndexed");
                break;

            case CommandType::DrawArrays:
                if (!current_pipe) {
                    std::fprintf(stderr,
                                 "[OpenGL Draw Warning] Skipping array draw without pipeline.\n");
                    break;
                }

                glBindVertexArray(m_empty_vao);
                glDrawArrays(GL_TRIANGLES, cmd.payload.draw_arrays.first_vertex,
                             cmd.payload.draw_arrays.vertex_count);
                glBindVertexArray(m_vao);

                check_gl_error("DrawArrays");
                break;
            }
        }
    }

    void update_buffer(BufferHandle handle, Slice<const Byte> data,
                       U32 offset = 0) noexcept override {
        if (data.is_empty()) {
            return;
        }

        GLuint *id = m_buffers.get_data(handle.key);
        if (!id) {
            std::fprintf(stderr, "[OpenGL Buffer Error] Invalid buffer handle in update_buffer.\n");
            return;
        }

        GLbitfield map_flags = GL_MAP_WRITE_BIT | (offset == 0 ? GL_MAP_INVALIDATE_BUFFER_BIT
                                                               : GL_MAP_INVALIDATE_RANGE_BIT);

        void *mapped = glMapNamedBufferRange(*id, offset, data.size(), map_flags);
        if (!mapped) {
            std::fprintf(stderr,
                         "[OpenGL Buffer Error] Failed to map buffer range in update_buffer.\n");
            check_gl_error("update_buffer map failure");
            return;
        }

        fr::mem::copy_raw_range(data.data(), data.size(), static_cast<Byte *>(mapped));

        if (glUnmapNamedBuffer(*id) == GL_FALSE) {
            std::fprintf(stderr,
                         "[OpenGL Buffer Error] Buffer contents became corrupted during update.\n");
        }

        check_gl_error("update_buffer");
    }

    Alloc *get_allocator() const noexcept {
        return m_alloc;
    }

    void *get_native_texture_handle(TextureHandle handle) noexcept override {
        if (!handle.is_valid()) {
            return nullptr;
        }

        OpenGLTexture *texture = m_textures.get_data(handle.key);
        return texture ? reinterpret_cast<void *>(static_cast<uintptr_t>(texture->id)) : nullptr;
    }

private:
    static GLenum cube_face_target(U32 layer) noexcept {
        return GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(layer);
    }

    void attach_texture_to_fbo(GLenum attachment, const RenderAttachment &view) noexcept {
        if (!view.texture.is_valid()) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
            return;
        }

        OpenGLTexture *texture = m_textures.get_data(view.texture.key);
        if (!texture || texture->id == 0) {
            std::fprintf(stderr, "[OpenGL RenderPass Error] Invalid attachment texture.\n");
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
            return;
        }

        if (texture->target == GL_TEXTURE_CUBE_MAP) {
            if (view.layer >= 6) {
                std::fprintf(stderr, "[OpenGL RenderPass Error] Invalid cubemap face index.\n");
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
                return;
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, cube_face_target(view.layer),
                                   texture->id, static_cast<GLint>(view.mip_level));
            return;
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture->id,
                               static_cast<GLint>(view.mip_level));
    }

private:
    GLuint m_fallback_fbo{0};
    GLuint m_vao{0};
    GLuint m_empty_vao{0};

    Alloc *m_alloc{nullptr};

    SlotMap<GLuint> m_buffers;
    SlotMap<OpenGLTexture> m_textures;
    SlotMap<GLuint> m_shaders;
    SlotMap<OpenGLPipeline> m_pipelines;

    OpenGLCommandBuffer m_runtime_cmd_buffer;
};

FR_API RenderDevice *create_opengl_render_device(Alloc *alloc) noexcept {
    void *mem = alloc->allocate(sizeof(OpenGLRenderDevice), alignof(OpenGLRenderDevice));
    return new (mem) OpenGLRenderDevice(alloc);
}

FR_API void destroy_opengl_render_device(RenderDevice *device) noexcept {
    if (!device) {
        return;
    }

    auto *opengl_device = static_cast<OpenGLRenderDevice *>(device);
    Alloc *alloc = opengl_device->get_allocator();

    opengl_device->~OpenGLRenderDevice();
    alloc->deallocate(opengl_device, sizeof(OpenGLRenderDevice), alignof(OpenGLRenderDevice));
}

} // namespace fr
