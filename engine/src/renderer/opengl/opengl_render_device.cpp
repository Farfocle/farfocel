#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slot_map.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_device.hpp"

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <iostream>
#include <unistd.h>

namespace fr {

struct OpenGLPipeline {
    GLuint program_id{0};
    GLenum cull_mode{GL_BACK};
    GLboolean depth_test{GL_TRUE};
    GLboolean depth_write{GL_TRUE};
    GLenum polygon_mode{GL_FILL};
    bool use_culling{true};
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
    DrawIndexed
};

struct OpenGLCommand {
    CommandType type;
    union {

        struct {
            TextureHandle color_targets[4];
            U32 num_colors;
            TextureHandle depth_target;
        } render_pass;

        struct {
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
            BufferHandle buffer;
            U32 slot;
        } storage_buffer;

        struct {
            U32 data[4]; // 16 bytes``
        } push_constants;

    } payload;
};

class OpenGLCommandBuffer : public CommandBuffer {
public:
    OpenGLCommandBuffer() = default;

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
        cmd.payload.render_pass.num_colors = fr::math::min(color_targets.size(), U64{4});

        for (U32 i = 0; i < cmd.payload.render_pass.num_colors; i++)
            cmd.payload.render_pass.color_targets[i] = color_targets[i];

        cmd.payload.render_pass.depth_target = depth_target;
        m_commands.push_back(cmd);
    }

    void end_render_pass() noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::EndRenderPass;
        m_commands.push_back(cmd);
    }

    void set_viewport(U32 width, U32 height) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::SetViewport;
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

    void draw_indexed(U32 index_count, U32 index_offset = 0,
                      U32 vertex_offset = 0) noexcept override {
        OpenGLCommand cmd{};
        cmd.type = CommandType::DrawIndexed;
        cmd.payload.draw.index_count = index_count;
        cmd.payload.draw.index_offset = index_offset;
        cmd.payload.draw.vertex_offset = vertex_offset;
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
        FR_ASSERT(data.size() <= 16, "Push constants need to be smaller or equal to 16 bytes");
        OpenGLCommand cmd{};
        cmd.type = CommandType::SetPushConstants;
        std::memcpy(cmd.payload.push_constants.data, data.data(), data.size());
        m_commands.push_back(cmd);
    }

private:
    DynamicArray<OpenGLCommand> m_commands;
};

class OpenGLRenderDevice : public RenderDevice {
public:
    explicit OpenGLRenderDevice(Alloc *alloc) noexcept
        : m_alloc(alloc) {
        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
            FR_ASSERT(false, "Couldn't initialize GLAD... that sucks");
        }

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        // FR LOG would be nice to have
        // this is so that the compiler doesn't cry bout the unused variables
        glDebugMessageCallback(
            [](GLenum /*source*/, GLenum /*type*/, GLuint /*id*/, GLenum severity,
               GLsizei /*length*/, const GLchar *message, const void * /*userParam*/) {
                if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM) {
                    std::cerr << "[OpenGL ERROR]: " << message << "\n";
                }
            },
            nullptr);

        glCreateFramebuffers(1, &m_fallback_fbo);
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
    }

    ~OpenGLRenderDevice() noexcept override {
        glDeleteFramebuffers(1, &m_fallback_fbo);
        glDeleteVertexArrays(1, &m_vao);
    }

    BufferHandle create_buffer(Slice<const Byte> data, bool is_dynamic) noexcept override {
        GLuint id = 0;
        glCreateBuffers(1, &id);
        GLbitfield flags = is_dynamic ? GL_DYNAMIC_STORAGE_BIT : 0;
        glNamedBufferStorage(id, data.size(), data.data(), flags);

        return BufferHandle{m_buffers.add(id)};
    }

    TextureHandle create_texture_2d(U32 width, U32 height, TextureFormat format,
                                    Slice<const Byte> data) noexcept override {
        GLuint id = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &id);

        GLenum internal_format = GL_RGBA8;
        GLenum data_format = GL_RGBA;
        GLenum data_type = GL_UNSIGNED_BYTE;

        switch (format) {
        case TextureFormat::R8G8B8A8_UNorm:
            internal_format = GL_RGBA8;
            data_format = GL_RGBA;
            data_type = GL_UNSIGNED_BYTE;
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
            internal_format = GL_DEPTH_COMPONENT32F;
            data_format = GL_DEPTH_COMPONENT;
            data_type = GL_FLOAT;
            break;
        }

        glTextureStorage2D(id, 1, internal_format, width, height);

        if (!data.is_empty())
            glTextureSubImage2D(id, 0, 0, 0, width, height, data_format, data_type, data.data());

        glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        return TextureHandle{m_textures.add(id)};
    }

    ShaderHandle create_shader(StringView vertex_src, StringView fragment_src) noexcept override {
        String vert_str = String::from_view(vertex_src);
        String frag_str = String::from_view(fragment_src);
        const char *vert_ptr = vert_str.data();
        const char *frag_ptr = frag_str.data();

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vert_ptr, nullptr);
        glCompileShader(vs);

        GLint success;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetShaderInfoLog(vs, 1024, nullptr, infoLog);
            FR_PANIC(infoLog);
        }

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &frag_ptr, nullptr);
        glCompileShader(fs);

        glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetShaderInfoLog(fs, 1024, nullptr, infoLog);
            FR_PANIC(infoLog);
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetProgramInfoLog(program, 1024, nullptr, infoLog);
            FR_PANIC(infoLog);
        }

        glDeleteShader(vs);
        glDeleteShader(fs);

        return ShaderHandle{m_shaders.add(program)};
    }

    RenderPipelineHandle
    create_render_pipeline(const RenderPipelineProperties &properties) noexcept override {
        OpenGLPipeline pipe{};

        GLuint *shader_id = m_shaders.get_data(properties.shader.key);
        if (shader_id) {
            pipe.program_id = *shader_id;
        }

        pipe.depth_test = properties.depth_test ? GL_TRUE : GL_FALSE;
        pipe.depth_write = properties.depth_write ? GL_TRUE : GL_FALSE;

        pipe.polygon_mode = properties.wireframe ? GL_LINE : GL_FILL;

        if (properties.cull_mode == CullMode::None) {
            pipe.use_culling = false;
        } else {
            pipe.use_culling = true;
            pipe.cull_mode = (properties.cull_mode == CullMode::Front) ? GL_FRONT : GL_BACK;
        }

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
        GLuint *id = m_textures.get_data(handle.key);
        if (id) {
            glDeleteTextures(1, id);
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

    void destory_pipeline(RenderPipelineHandle handle) noexcept override {
        m_pipelines.erase(handle.key);
    }

    CommandBuffer *adopt_command_buffer() noexcept override {
        m_runtime_cmd_buffer.clear_commands();
        return &m_runtime_cmd_buffer;
    }

    void submit_command_buffer(CommandBuffer *cmd_buffer) noexcept override {
        auto *gl_cmd_buffer = static_cast<OpenGLCommandBuffer *>(cmd_buffer);
        const auto &stream = gl_cmd_buffer->get_commands();

        for (USize i = 0; i < stream.size(); ++i) {
            const auto &cmd = stream[i];

            switch (cmd.type) {
            case CommandType::BeginRenderPass: {
                if (cmd.payload.render_pass.num_colors == 0 &&
                    !cmd.payload.render_pass.depth_target.is_valid()) {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                } else {
                    glBindFramebuffer(GL_FRAMEBUFFER, m_fallback_fbo);
                    GLenum draw_buffers[4] = {GL_NONE};

                    for (U32 c = 0; c < cmd.payload.render_pass.num_colors; ++c) {
                        GLuint gl_tex = *m_textures.get_data_unsafe(
                            cmd.payload.render_pass.color_targets[c].key);
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + c,
                                               GL_TEXTURE_2D, gl_tex, 0);
                        draw_buffers[c] = GL_COLOR_ATTACHMENT0 + c;
                    }
                    glDrawBuffers(cmd.payload.render_pass.num_colors, draw_buffers);

                    if (cmd.payload.render_pass.depth_target.is_valid()) {
                        GLuint gl_depth =
                            *m_textures.get_data_unsafe(cmd.payload.render_pass.depth_target.key);
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                               gl_depth, 0);
                    }

                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                }
                break;
            }
            case CommandType::EndRenderPass: {
                break;
            }

            case CommandType::SetViewport: {
                glViewport(0, 0, cmd.payload.viewport.width, cmd.payload.viewport.height);
                break;
            }

            case CommandType::SetPipeline: {
                const OpenGLPipeline &pipeline =
                    *m_pipelines.get_data_unsafe(cmd.payload.pipeline.pipeline.key);
                glUseProgram(pipeline.program_id);

                if (pipeline.depth_test)
                    glEnable(GL_DEPTH_TEST);
                else
                    glDisable(GL_DEPTH_TEST);

                glDepthMask(pipeline.depth_write);
                glPolygonMode(GL_FRONT_AND_BACK, pipeline.polygon_mode);

                if (pipeline.use_culling) {
                    glEnable(GL_CULL_FACE);
                    glCullFace(pipeline.cull_mode);
                } else {
                    glDisable(GL_CULL_FACE);
                }
                break;
            }

            case CommandType::BindVertexBuffer: {
                GLuint vbo = *m_buffers.get_data_unsafe(cmd.payload.vertex_buffer.vbo.key);

                glVertexArrayVertexBuffer(m_vao, 0, vbo, 0, cmd.payload.vertex_buffer.stride);

                // position
                glEnableVertexArrayAttrib(m_vao, 0);
                glVertexArrayAttribFormat(m_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
                glVertexArrayAttribBinding(m_vao, 0, 0);

                // normals
                glEnableVertexArrayAttrib(m_vao, 1);
                glVertexArrayAttribFormat(m_vao, 1, 3, GL_FLOAT, GL_FALSE, 12);
                glVertexArrayAttribBinding(m_vao, 1, 0);

                // uv
                glEnableVertexArrayAttrib(m_vao, 2);
                glVertexArrayAttribFormat(m_vao, 2, 2, GL_FLOAT, GL_FALSE, 24);
                glVertexArrayAttribBinding(m_vao, 2, 0);

                break;
            }
            case CommandType::BindStorageBuffer: {
                GLuint ssbo = *m_buffers.get_data_unsafe(cmd.payload.storage_buffer.buffer.key);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, cmd.payload.storage_buffer.slot, ssbo);
                break;
            }

                // opengl does not support push constants, but this is a workaround, and more so, it
                // will work fully properly when vulkan comes
            case CommandType::SetPushConstants: {
                GLint current_program;
                glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
                GLint loc = glGetUniformLocation(current_program, "u_transform_idx");
                if (loc != -1) {
                    glUniform1ui(loc, cmd.payload.push_constants.data[0]);
                }
                break;
            }

            case CommandType::BindIndexBuffer: {
                GLuint ibo = *m_buffers.get_data_unsafe(cmd.payload.index_buffer.ibo.key);
                glVertexArrayElementBuffer(m_vao, ibo);
                break;
            }

            case CommandType::BindTexture: {
                GLuint tex = *m_textures.get_data_unsafe(cmd.payload.texture.texture.key);
                glBindTextureUnit(cmd.payload.texture.slot, tex);
                break;
            }

            case CommandType::DrawIndexed: {
                glDrawElementsBaseVertex(GL_TRIANGLES, cmd.payload.draw.index_count,
                                         GL_UNSIGNED_INT,
                                         reinterpret_cast<void *>(static_cast<USize>(
                                             cmd.payload.draw.index_offset * sizeof(U32))),
                                         cmd.payload.draw.vertex_offset);
                break;
            }
            }
        }
    }

    void update_buffer(BufferHandle handle, Slice<const Byte> data,
                       U32 offset = 0) noexcept override {
        if (data.is_empty())
            return;
        GLuint *id =
            m_buffers.get_data(handle.key); // cannot use unsafe version, since not a drawing loow
        // this sucks
        if (id)
            glNamedBufferSubData(*id, offset, data.size(), data.data());
    }

    Alloc *get_allocator() const noexcept {
        return m_alloc;
    }

private:
    GLuint m_fallback_fbo{0};
    GLuint m_vao{0};
    Alloc *m_alloc{nullptr};

    SlotMap<GLuint> m_buffers;
    SlotMap<GLuint> m_textures;
    SlotMap<GLuint> m_shaders;
    SlotMap<OpenGLPipeline> m_pipelines;

    OpenGLCommandBuffer m_runtime_cmd_buffer;
};

FR_API RenderDevice *create_opengl_render_device(Alloc *alloc) noexcept {
    FR_ASSERT(alloc != nullptr, "RenderDevice requiers allocator");
    void *mem = alloc->allocate(sizeof(OpenGLRenderDevice), alignof(OpenGLRenderDevice));
    return new (mem) OpenGLRenderDevice(alloc);
}

FR_API void destroy_opengl_render_device(RenderDevice *device) noexcept {
    if (!device)
        return;

    auto *gl_device = static_cast<OpenGLRenderDevice *>(device);

    Alloc *alloc = gl_device->get_allocator();
    gl_device->~OpenGLRenderDevice();

    alloc->deallocate(gl_device, sizeof(OpenGLRenderDevice), alignof(OpenGLRenderDevice));
}

} // namespace fr
