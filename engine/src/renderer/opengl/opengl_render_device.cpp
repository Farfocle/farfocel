/**
 * @file opengl_render_device.cpp
 * @author Tfoedy
 * @brief OpenGL render device backend.
 */

#include <SDL3/SDL.h>
#include <cstdint>
#include <glad/gl.h>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/slot_map.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/logger/logger.hpp"
#include "fr/renderer/render_device.hpp"

namespace fr {

struct OpenGLBuffer {
    GLuint id{0};
    USize size{0};
    BufferUsage usage{BufferUsage::Static};
};

struct OpenGLTexture {
    GLuint id{0};
    GLenum target{GL_TEXTURE_2D};
    TextureFormat format{TextureFormat::R8G8B8A8_UNorm};
};

struct OpenGLPipeline {
    GLuint program_id{0};

    GLenum cull_mode{GL_BACK};
    GLenum polygon_mode{GL_FILL};

    GLboolean depth_test{GL_TRUE};
    GLboolean depth_write{GL_TRUE};

    bool use_culling{true};
    BlendMode blend_mode{BlendMode::None};

    GLint loc_transform_idx{-1};
    GLint loc_material_idx{-1};
    GLint loc_cascade_idx{-1};
    GLint loc_shadow_idx{-1};
};

enum class OpenGLCommandKind : U8 {
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
    OpenGLCommandKind kind{OpenGLCommandKind::EndRenderPass};

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
        FR_ASSERT(alloc, "allocator must be non-null");

        m_commands.reserve(2048);
    }

    void clear() noexcept {
        m_commands.clear();
    }

    [[nodiscard]] const DynamicArray<OpenGLCommand> &commands() const noexcept {
        return m_commands;
    }

    void begin_render_pass(Slice<const TextureHandle> color_targets,
                           TextureHandle depth_target) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::BeginRenderPass;

        const USize color_count = fr::math::min<USize>(color_targets.size(), 4);
        cmd.payload.render_pass.num_colors = static_cast<U32>(color_count);

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
        cmd.kind = OpenGLCommandKind::BeginRenderPass;

        const USize color_count = fr::math::min<USize>(color_targets.size(), 4);
        cmd.payload.render_pass.num_colors = static_cast<U32>(color_count);

        for (U32 i = 0; i < cmd.payload.render_pass.num_colors; ++i) {
            cmd.payload.render_pass.color_targets[i] = color_targets[i];
        }

        cmd.payload.render_pass.depth_target = depth_target;

        m_commands.push_back(cmd);
    }

    void end_render_pass() noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::EndRenderPass;
        m_commands.push_back(cmd);
    }

    void set_viewport(U32 x, U32 y, U32 width, U32 height) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::SetViewport;
        cmd.payload.viewport.x = x;
        cmd.payload.viewport.y = y;
        cmd.payload.viewport.width = width;
        cmd.payload.viewport.height = height;
        m_commands.push_back(cmd);
    }

    void set_pipeline(RenderPipelineHandle pipeline) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::SetPipeline;
        cmd.payload.pipeline.pipeline = pipeline;
        m_commands.push_back(cmd);
    }

    void bind_vertex_buffer(BufferHandle vbo, U32 stride) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::BindVertexBuffer;
        cmd.payload.vertex_buffer.vbo = vbo;
        cmd.payload.vertex_buffer.stride = stride;
        m_commands.push_back(cmd);
    }

    void bind_index_buffer(BufferHandle ibo) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::BindIndexBuffer;
        cmd.payload.index_buffer.ibo = ibo;
        m_commands.push_back(cmd);
    }

    void bind_texture(TextureHandle texture, U32 slot) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::BindTexture;
        cmd.payload.texture.texture = texture;
        cmd.payload.texture.slot = slot;
        m_commands.push_back(cmd);
    }

    void bind_storage_buffer(BufferHandle buffer, U32 slot) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::BindStorageBuffer;
        cmd.payload.storage_buffer.buffer = buffer;
        cmd.payload.storage_buffer.slot = slot;
        m_commands.push_back(cmd);
    }

    void set_push_constants(Slice<const Byte> data) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::SetPushConstants;

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
        cmd.kind = OpenGLCommandKind::DrawIndexed;
        cmd.payload.draw.index_count = index_count;
        cmd.payload.draw.index_offset = index_offset;
        cmd.payload.draw.vertex_offset = vertex_offset;
        m_commands.push_back(cmd);
    }

    void draw_arrays(U32 vertex_count, U32 first_vertex = 0) noexcept override {
        OpenGLCommand cmd{};
        cmd.kind = OpenGLCommandKind::DrawArrays;
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
        FR_LOG_ERR("[OpenGL Error] {}: {} ({})", label, gl_error_to_string(error),
                   static_cast<U32>(error));
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
        FR_LOG_ERR("[OpenGL FBO Error] {}: {} ({})", label, framebuffer_status_to_string(status),
                   static_cast<U32>(status));
        return false;
    }

    return true;
}

StringView shader_debug_name_or_fallback(StringView debug_name) noexcept {
    if (!debug_name.is_empty()) {
        return debug_name;
    }

    return StringView("<unnamed shader>");
}

String read_shader_info_log(GLuint shader) noexcept {
    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

    if (info_len <= 0) {
        return String{};
    }

    String log;
    log.grow_default(static_cast<USize>(info_len));
    glGetShaderInfoLog(shader, info_len, nullptr, log.data());

    if (log.size() > 0 && log.back() == '\0') {
        log.shrink(log.size() - 1);
    }

    return log;
}

String read_program_info_log(GLuint program) noexcept {
    GLint info_len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);

    if (info_len <= 0) {
        return String{};
    }

    String log;
    log.grow_default(static_cast<USize>(info_len));
    glGetProgramInfoLog(program, info_len, nullptr, log.data());

    if (log.size() > 0 && log.back() == '\0') {
        log.shrink(log.size() - 1);
    }

    return log;
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
        FR_ASSERT(alloc, "allocator must be non-null");

        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
            FR_PANIC("Couldn't init GLAD");
        }

        glCreateFramebuffers(1, &m_fallback_fbo);
        glGenVertexArrays(1, &m_vao);
        glGenVertexArrays(1, &m_empty_vao);

        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    ~OpenGLRenderDevice() noexcept override {
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        destroy_remaining_resources();

        if (m_fallback_fbo != 0) {
            glDeleteFramebuffers(1, &m_fallback_fbo);
            m_fallback_fbo = 0;
        }

        if (m_vao != 0) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }

        if (m_empty_vao != 0) {
            glDeleteVertexArrays(1, &m_empty_vao);
            m_empty_vao = 0;
        }
    }

    TextureHandle create_texture(const TextureDesc &desc) noexcept override {
        if (desc.width == 0 || desc.height == 0) {
            FR_LOG_ERR("[OpenGL Texture Error] Cannot create a zero-sized texture.");
            return TextureHandle{};
        }

        if (desc.dimension == TextureDimension::Cube && desc.width != desc.height) {
            FR_LOG_ERR("[OpenGL Texture Error] Cubemap faces must be square.");
            return TextureHandle{};
        }

        if (desc.dimension == TextureDimension::Cube && !desc.initial_data.is_empty()) {
            FR_LOG_ERR("[OpenGL Texture Error] Cubemap initial data is not supported yet.");
            return TextureHandle{};
        }

        GLenum gl_target = GL_TEXTURE_2D;
        if (desc.dimension == TextureDimension::Cube) {
            gl_target = GL_TEXTURE_CUBE_MAP;
        }

        GLuint id = 0;
        glCreateTextures(gl_target, 1, &id);

        if (id == 0) {
            FR_LOG_ERR("[OpenGL Texture Error] glCreateTextures returned 0.");
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

        U32 allocated_mips = fr::math::max<U32>(desc.mip_levels, 1);

        if (should_generate_runtime_mips(desc)) {
            allocated_mips = full_mip_count(desc.width, desc.height);
        }

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

        check_gl_error("create_texture(TextureDesc)");

        return TextureHandle{m_textures.add(texture)};
    }

    BufferHandle create_buffer(const BufferDesc &desc) noexcept override {
        USize buffer_size = desc.size;

        if (buffer_size == 0 && !desc.initial_data.is_empty()) {
            buffer_size = desc.initial_data.size();
        }

        if (buffer_size == 0) {
            FR_LOG_ERR("[OpenGL Buffer Error] Cannot create a zero-sized buffer.");
            return BufferHandle{};
        }

        if (!desc.initial_data.is_empty() && desc.initial_data.size() > buffer_size) {
            FR_LOG_ERR("[OpenGL Buffer Error] Initial buffer data is larger than buffer size.");
            return BufferHandle{};
        }

        GLuint id = 0;
        glCreateBuffers(1, &id);

        if (id == 0) {
            FR_LOG_ERR("[OpenGL Buffer Error] glCreateBuffers returned 0.");
            return BufferHandle{};
        }

        GLenum gl_usage = GL_STATIC_DRAW;
        if (desc.usage == BufferUsage::Dynamic) {
            gl_usage = GL_DYNAMIC_DRAW;
        }

        const void *initial_data =
            desc.initial_data.is_empty() ? nullptr : desc.initial_data.data();

        glNamedBufferData(id, buffer_size, initial_data, gl_usage);

        OpenGLBuffer buffer{};
        buffer.id = id;
        buffer.size = buffer_size;
        buffer.usage = desc.usage;

        check_gl_error("create_buffer(BufferDesc)");

        return BufferHandle{m_buffers.add(buffer)};
    }

    ShaderHandle create_shader(StringView vertex_src, StringView fragment_src,
                               StringView debug_name = {}) noexcept override {
        const StringView shader_name = shader_debug_name_or_fallback(debug_name);

        if (vertex_src.is_empty() || fragment_src.is_empty()) {
            FR_LOG_ERR("[OpenGL Shader Error] Cannot create shader '{}' from empty source.",
                       shader_name);
            return ShaderHandle{};
        }

        const char *vertex_ptr = vertex_src.data();
        const char *fragment_ptr = fragment_src.data();

        const GLint vertex_len = static_cast<GLint>(vertex_src.size());
        const GLint fragment_len = static_cast<GLint>(fragment_src.size());

        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &vertex_ptr, &vertex_len);
        glCompileShader(vertex_shader);

        GLint vertex_compiled = GL_FALSE;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);

        if (vertex_compiled != GL_TRUE) {
            String log = read_shader_info_log(vertex_shader);
            FR_LOG_ERR("[OpenGL Shader Error] Vertex shader compilation failed for '{}':\n{}",
                       shader_name, log.view());

            glDeleteShader(vertex_shader);
            return ShaderHandle{};
        }

        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &fragment_ptr, &fragment_len);
        glCompileShader(fragment_shader);

        GLint fragment_compiled = GL_FALSE;
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_compiled);

        if (fragment_compiled != GL_TRUE) {
            String log = read_shader_info_log(fragment_shader);
            FR_LOG_ERR("[OpenGL Shader Error] Vertex shader compilation failed for '{}':\n{}",
                       shader_name, log.view());

            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);
            return ShaderHandle{};
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);

        glDetachShader(program, vertex_shader);
        glDetachShader(program, fragment_shader);

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        if (linked != GL_TRUE) {
            String log = read_program_info_log(program);
            FR_LOG_ERR("[OpenGL Shader Error] Program linking failed for '{}':\n{}", shader_name,
                       log.view());

            glDeleteProgram(program);
            return ShaderHandle{};
        }

        check_gl_error("create_shader");

        return ShaderHandle{m_shaders.add(program)};
    }

    RenderPipelineHandle
    create_render_pipeline(const RenderPipelineProperties &properties) noexcept override {
        GLuint *shader_id = m_shaders.get_data(properties.shader.key);
        if (!shader_id || *shader_id == 0) {
            FR_LOG_ERR(
                "[OpenGL Pipeline Error] Cannot create pipeline from invalid shader handle.");
            return RenderPipelineHandle{};
        }

        OpenGLPipeline pipe{};
        pipe.program_id = *shader_id;

        pipe.loc_transform_idx = glGetUniformLocation(pipe.program_id, "u_transform_idx");
        pipe.loc_material_idx = glGetUniformLocation(pipe.program_id, "u_material_idx");
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
        OpenGLBuffer *buffer = m_buffers.get_data(handle.key);
        if (!buffer) {
            return;
        }

        if (buffer->id != 0) {
            glDeleteBuffers(1, &buffer->id);
            buffer->id = 0;
            buffer->size = 0;
        }

        m_buffers.erase(handle.key);
    }

    void destroy_texture(TextureHandle handle) noexcept override {
        OpenGLTexture *texture = m_textures.get_data(handle.key);
        if (!texture) {
            return;
        }

        if (texture->id != 0) {
            glDeleteTextures(1, &texture->id);
            texture->id = 0;
        }

        m_textures.erase(handle.key);
    }

    void destroy_shader(ShaderHandle handle) noexcept override {
        GLuint *program = m_shaders.get_data(handle.key);
        if (!program) {
            return;
        }

        if (*program != 0) {
            glDeleteProgram(*program);
            *program = 0;
        }

        m_shaders.erase(handle.key);
    }

    void destroy_pipeline(RenderPipelineHandle handle) noexcept override {
        if (!handle.is_valid()) {
            return;
        }

        m_pipelines.erase(handle.key);
    }

    CommandBuffer *adopt_command_buffer() noexcept override {
        m_runtime_cmd_buffer.clear();
        return &m_runtime_cmd_buffer;
    }

    void submit_command_buffer(CommandBuffer *cmd_buffer) noexcept override {
        if (!cmd_buffer) {
            FR_LOG_ERR("[OpenGL Command Error] Cannot submit a null command buffer.");
            return;
        }

        glBindVertexArray(m_vao);

        auto *gl_cmd_buffer = static_cast<OpenGLCommandBuffer *>(cmd_buffer);

        const DynamicArray<OpenGLCommand> &stream = gl_cmd_buffer->commands();

        const OpenGLPipeline *current_pipe = nullptr;

        for (USize i = 0; i < stream.size(); ++i) {
            const OpenGLCommand &cmd = stream[i];

            switch (cmd.kind) {
            case OpenGLCommandKind::BeginRenderPass: {
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

                    const bool is_depth_stencil =
                        depth_texture && depth_texture->format == TextureFormat::Depth24_Stencil8;

                    if (is_depth_stencil) {
                        attach_texture_to_fbo(GL_DEPTH_ATTACHMENT, RenderAttachment{});
                        attach_texture_to_fbo(GL_DEPTH_STENCIL_ATTACHMENT,
                                              cmd.payload.render_pass.depth_target);
                    } else {
                        attach_texture_to_fbo(GL_DEPTH_STENCIL_ATTACHMENT, RenderAttachment{});
                        attach_texture_to_fbo(GL_DEPTH_ATTACHMENT,
                                              cmd.payload.render_pass.depth_target);
                    }
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

            case OpenGLCommandKind::EndRenderPass:
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDrawBuffer(GL_BACK);
                glReadBuffer(GL_BACK);
                check_gl_error("EndRenderPass");
                break;

            case OpenGLCommandKind::SetViewport:
                glViewport(cmd.payload.viewport.x, cmd.payload.viewport.y,
                           cmd.payload.viewport.width, cmd.payload.viewport.height);
                break;

            case OpenGLCommandKind::SetPipeline: {
                current_pipe = m_pipelines.get_data(cmd.payload.pipeline.pipeline.key);

                if (!current_pipe || current_pipe->program_id == 0) {
                    FR_LOG_ERR("[OpenGL Pipeline Error] Tried to bind an invalid pipeline.");
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

            case OpenGLCommandKind::BindVertexBuffer: {
                OpenGLBuffer *vbo = m_buffers.get_data(cmd.payload.vertex_buffer.vbo.key);
                if (!vbo || vbo->id == 0) {
                    FR_LOG_ERR("[OpenGL Buffer Error] Invalid vertex buffer handle.");
                    break;
                }

                glVertexArrayVertexBuffer(m_vao, 0, vbo->id, 0, cmd.payload.vertex_buffer.stride);

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

            case OpenGLCommandKind::BindStorageBuffer: {
                OpenGLBuffer *ssbo = m_buffers.get_data(cmd.payload.storage_buffer.buffer.key);
                if (!ssbo || ssbo->id == 0) {
                    FR_LOG_ERR("[OpenGL Buffer Error] Invalid storage buffer handle.");
                    break;
                }

                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, cmd.payload.storage_buffer.slot,
                                 ssbo->id);

                check_gl_error("BindStorageBuffer");
                break;
            }

            case OpenGLCommandKind::SetPushConstants: {
                if (!current_pipe) {
                    break;
                }

                if (current_pipe->loc_transform_idx != -1) {
                    glUniform1ui(current_pipe->loc_transform_idx,
                                 cmd.payload.push_constants.data[0]);
                }

                if (current_pipe->loc_material_idx != -1) {
                    glUniform1ui(current_pipe->loc_material_idx,
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

            case OpenGLCommandKind::BindIndexBuffer: {
                OpenGLBuffer *ibo = m_buffers.get_data(cmd.payload.index_buffer.ibo.key);
                if (!ibo || ibo->id == 0) {
                    FR_LOG_ERR("[OpenGL Buffer Error] Invalid index buffer handle.");
                    break;
                }

                glVertexArrayElementBuffer(m_vao, ibo->id);

                check_gl_error("BindIndexBuffer");
                break;
            }

            case OpenGLCommandKind::BindTexture: {
                OpenGLTexture *texture = m_textures.get_data(cmd.payload.texture.texture.key);
                if (!texture || texture->id == 0) {
                    FR_LOG_ERR("[OpenGL Texture Error] Invalid texture handle at slot {}.",
                               cmd.payload.texture.slot);
                    break;
                }

                glBindTextureUnit(cmd.payload.texture.slot, texture->id);
                check_gl_error("BindTexture");
                break;
            }

            case OpenGLCommandKind::DrawIndexed:
                if (!current_pipe) {
                    FR_LOG_WARN("[OpenGL Draw Warning] Skipping indexed draw without pipeline.");
                    break;
                }

                glDrawElementsBaseVertex(
                    GL_TRIANGLES, cmd.payload.draw.index_count, GL_UNSIGNED_INT,
                    reinterpret_cast<void *>(static_cast<USize>(cmd.payload.draw.index_offset) *
                                             sizeof(U32)),
                    cmd.payload.draw.vertex_offset);

                check_gl_error("DrawIndexed");
                break;

            case OpenGLCommandKind::DrawArrays:
                if (!current_pipe) {
                    FR_LOG_WARN("[OpenGL Draw Warning] Skipping array draw without pipeline.");
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

        OpenGLBuffer *buffer = m_buffers.get_data(handle.key);
        if (!buffer || buffer->id == 0) {
            FR_LOG_ERR("[OpenGL Buffer Error] Invalid buffer handle in update_buffer.");
            return;
        }

        const USize byte_offset = static_cast<USize>(offset);

        if (byte_offset > buffer->size || data.size() > buffer->size - byte_offset) {
            FR_LOG_ERR(
                "[OpenGL Buffer Error] update_buffer range out of bounds. Offset: {}, size: {}, "
                "buffer size: {}.",
                byte_offset, data.size(), buffer->size);
            return;
        }

        GLbitfield map_flags = GL_MAP_WRITE_BIT | (offset == 0 ? GL_MAP_INVALIDATE_BUFFER_BIT
                                                               : GL_MAP_INVALIDATE_RANGE_BIT);

        void *mapped = glMapNamedBufferRange(buffer->id, offset, data.size(), map_flags);
        if (!mapped) {
            FR_LOG_ERR("[OpenGL Buffer Error] Failed to map buffer range in update_buffer.");
            check_gl_error("update_buffer map failure");
            return;
        }

        fr::mem::copy_raw_range(data.data(), data.size(), static_cast<Byte *>(mapped));

        if (glUnmapNamedBuffer(buffer->id) == GL_FALSE) {
            FR_LOG_ERR("[OpenGL Buffer Error] Buffer contents became corrupted during update.");
        }

        check_gl_error("update_buffer");
    }

    [[nodiscard]] Alloc *allocator() const noexcept {
        return m_alloc;
    }

    void *native_texture_handle(TextureHandle handle) noexcept override {
        if (!handle.is_valid()) {
            return nullptr;
        }

        OpenGLTexture *texture = m_textures.get_data(handle.key);
        if (!texture || texture->id == 0) {
            return nullptr;
        }

        return reinterpret_cast<void *>(static_cast<uintptr_t>(texture->id));
    }

private:
    void destroy_remaining_resources() noexcept {
        m_buffers.for_each_alive([](SlotKey, OpenGLBuffer &buffer) noexcept {
            if (buffer.id != 0) {
                glDeleteBuffers(1, &buffer.id);
                buffer.id = 0;
                buffer.size = 0;
            }
        });

        m_textures.for_each_alive([](SlotKey, OpenGLTexture &texture) noexcept {
            if (texture.id != 0) {
                glDeleteTextures(1, &texture.id);
                texture.id = 0;
            }
        });

        m_shaders.for_each_alive([](SlotKey, GLuint &program) noexcept {
            if (program != 0) {
                glDeleteProgram(program);
                program = 0;
            }
        });

        m_pipelines.clear();
    }

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
            FR_LOG_ERR("[OpenGL RenderPass Error] Invalid attachment texture.");
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
            return;
        }

        if (texture->target == GL_TEXTURE_CUBE_MAP) {
            if (view.layer >= 6) {
                FR_LOG_ERR("[OpenGL RenderPass Error] Invalid cubemap face index.");
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
    [[nodiscard]] bool is_depth_texture_format(TextureFormat format) noexcept {
        return format == TextureFormat::Depth24_Stencil8 ||
               format == TextureFormat::Depth32_Float ||
               format == TextureFormat::Depth32_Float_Shadow;
    }

    [[nodiscard]] U32 full_mip_count(U32 width, U32 height) noexcept {
        U32 max_dim = fr::math::max<U32>(width, height);
        U32 levels = 1;

        while (max_dim > 1) {
            max_dim /= 2;
            ++levels;
        }

        return levels;
    }

    [[nodiscard]] bool should_generate_runtime_mips(const TextureDesc &desc) noexcept {
        if (desc.initial_data.is_empty()) {
            return false;
        }

        if (desc.dimension != TextureDimension::Texture2D) {
            return false;
        }

        if (is_depth_texture_format(desc.format)) {
            return false;
        }

        return true;
    }

    GLuint m_fallback_fbo{0};
    GLuint m_vao{0};
    GLuint m_empty_vao{0};

    Alloc *m_alloc{nullptr};

    SlotMap<OpenGLBuffer> m_buffers;
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
    Alloc *alloc = opengl_device->allocator();

    opengl_device->~OpenGLRenderDevice();
    alloc->deallocate(opengl_device, sizeof(OpenGLRenderDevice), alignof(OpenGLRenderDevice));
}

} // namespace fr
