/**
 * @file render_device.hpp
 * @author Tfoedy
 *
 * @brief Low-level GPU code abstraction (RHI)
 * Manages GPU memory allocations, command generation.
 * Abstracts API specific code.
 */

#pragma once

#include "fr/core/slice.hpp"
#include "fr/core/slot_map.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/strong_handle.hpp"
#include "fr/core/typedefs.hpp"

#include <glm/glm.hpp>

namespace fr {
// STRONGLY TYPED HANDLES
struct BufferTag {};
using BufferHandle = StrongHandle<BufferTag>;

struct TextureTag {};
using TextureHandle = StrongHandle<TextureTag>;

struct ShaderTag {};
using ShaderHandle = StrongHandle<ShaderTag>;

struct PipelineTag {};
using RenderPipelineHandle = StrongHandle<PipelineTag>;

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
// ----------------

/**
 * @brief Supported texture formats for GPU allocation.
 */
enum class TextureFormat : U8 {
    R8G8B8A8_UNorm,
    R8G8B8A8_SRGB,
    R16G16B16A16_Float,
    R32G32B32A32_Float,
    Depth24_Stencil8,
    Depth32_Float
};

/**
 * @brief Polygon culling modes.
 */
enum class CullMode : U8 { None, Front, Back };
/**
 * @brief Properties used to create specify a graphics pipeline.
 */
struct RenderPipelineProperties {
    /// Handle to the compiled shader program
    ShaderHandle shader;
    CullMode cull_mode{CullMode::Back};
    /// Enable Z-buffer depth testing
    bool depth_test{true};
    /// Enable writing tothe Z-buffer
    bool depth_write{true};
    bool wireframe{false};
};

/**
 * @brief Interface for recording GPU rendering commands.
 */
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;

    /**
     * @brief Begins a render pass with specified render targets.
     * * @param color_targets A slice of texture handles to serve as color attachments.
     * @param depth_target A texture handle to serve as the depth attachment.
     */
    virtual void begin_render_pass(Slice<const TextureHandle> color_targets,
                                   TextureHandle depth_target) noexcept = 0;
    /**
     * @brief Ends the current render pass.
     */
    virtual void end_render_pass() noexcept = 0;

    /**
     * @brief Sets the viewport dimensions.
     * * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     */
    virtual void set_viewport(U32 width, U32 height) noexcept = 0;
    /**
     * @brief Binds a graphics pipeline for subsequent draw calls.
     * * @param pipeline Handle to the pipeline.
     */
    virtual void set_pipeline(RenderPipelineHandle pipeline) noexcept = 0;

    /**
     * @brief Binds a vertex buffer to the pipeline.
     * * @param vbo Handle to the vertex buffer.
     * @param stride Stride in bytes between consecutive vertices.
     */
    virtual void bind_vertex_buffer(BufferHandle vbo, U32 stride) noexcept = 0;
    /**
     * @brief Binds an index buffer to the pipeline.
     * * @param ibo Handle to the index buffer.
     */
    virtual void bind_index_buffer(BufferHandle ibo) noexcept = 0;
    /**
     * @brief Binds a texture to a specific binding slot.
     * * @param texture Handle to the texture.
     * @param slot The binding slot index.
     */
    virtual void bind_texture(TextureHandle texture, U32 slot) noexcept = 0;

    /**
     * @brief Binds a Shader Storage Buffer Object (SSBO).
     * * @param buffer Handle to the SSBO.
     * @param slot The binding slot index.
     */
    virtual void bind_storage_buffer(BufferHandle buffer, U32 slot) noexcept = 0;
    /**
     * @brief Uploads push constants (small amounts of data) directly to the shader.
     * * @param data Slice containing the byte data (max 16 bytes).
     */
    virtual void set_push_constants(Slice<const Byte> data) noexcept = 0;

    /**
     * @brief Issues an indexed draw command.
     * * @param index_count Number of indices to draw.
     * @param index_offset Starting offset in the index buffer.
     * @param vertex_offset Base vertex value added to each index.
     */
    virtual void draw_indexed(U32 index_count, U32 index_offset = 0,
                              U32 vertex_offset = 0) noexcept = 0;

    /**
     * @brief Issues a non-indexed hardware draw command. Useful for or procedurally generated
     * geometry.
     * @param vertex_count Number of sequential vertices to draw.
     * @param first_vertex Starting vertex index.
     */
    virtual void draw_arrays(U32 vertex_count, U32 first_vertex = 0) noexcept = 0;
};

class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    /**
     * @brief Creates a generic data buffer (VBO/IBO/SSBO).
     * * @param data Slice of initial data to upload.
     * @param is_dynamic Hint that the buffer will be updated frequently.
     * @return Handle to the created buffer.
     */
    virtual BufferHandle create_buffer(Slice<const Byte> data, bool is_dynamic) noexcept = 0;
    /**
     * @brief Updates an existing buffer with new data.
     * * @param handle Handle to the buffer.
     * @param data Slice of new data to upload.
     * @param offset Byte offset from the start of the buffer.
     */
    virtual void update_buffer(BufferHandle handle, Slice<const Byte> data,
                               U32 offset = 0) noexcept = 0;
    /**
     * @brief Creates a 2D texture.
     * * @param width Texture width.
     * @param height Texture height.
     * @param format Texture internal format.
     * @param data Initial pixel data.
     * @return Handle to the created texture.
     */
    virtual TextureHandle create_texture_2d(U32 width, U32 height, TextureFormat format,
                                            Slice<const Byte> data = {}) noexcept = 0;
    /**
     * @brief Compiles and links a shader program.
     * * @param vertex_src Vertex shader source code.
     * @param fragment_src Fragment shader source code.
     * @return Handle to the compiled shader program.
     */
    virtual ShaderHandle create_shader(StringView vertex_src, StringView fragment_src) noexcept = 0;
    /**
     * @brief Creates a new render pipeline with specified parameters.
     * @param properties Pipeline configuration (shaders, depth state).
     * @return Safe handle to the graphics pipeline.
     */
    virtual RenderPipelineHandle
    create_render_pipeline(const RenderPipelineProperties &properties) noexcept = 0;

    /**
     * @brief Destroys a buffer resource.
     * * @param handle Handle to the buffer to destroy.
     */
    virtual void destroy_buffer(BufferHandle handle) noexcept = 0;
    /**
     * @brief Destroys a texture resource.
     * * @param handle Handle to the texture to destroy.
     */
    virtual void destroy_texture(TextureHandle handle) noexcept = 0;
    /**
     * @brief Destroys a shader resource.
     * * @param handle Handle to the shader to destroy.
     */
    virtual void destroy_shader(ShaderHandle handle) noexcept = 0;
    /**
     * @brief Destroys a render pipeline resource.
     * * @param handle Handle to the pipeline to destroy.
     */
    virtual void destory_pipeline(RenderPipelineHandle handle) noexcept = 0;

    /**
     * @brief Retrieves a clean command buffer ready for recording.
     * * @return Pointer to a CommandBuffer interface.
     */
    virtual CommandBuffer *adopt_command_buffer() noexcept = 0;

    /**
     * @brief Submits a recorded command buffer for execution on the GPU.
     * * @param cmd_buffer Pointer to the recorded CommandBuffer.
     */
    virtual void submit_command_buffer(CommandBuffer *cmd_buffer) noexcept = 0;
};

/**
 * @brief Factory function to create an OpenGL backend RenderDevice.
 * * @param alloc Allocator to use for internal tracking.
 * @return Pointer to the newly created RenderDevice.
 */
FR_API RenderDevice *create_opengl_render_device(Alloc *alloc) noexcept;
/**
 * @brief Destroys an OpenGL RenderDevice and frees its memory.
 * * @param device Pointer to the device to destroy.
 */
FR_API void destroy_opengl_render_device(RenderDevice *device) noexcept;

} // namespace fr
