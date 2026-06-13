/**
 * @file render_device.hpp
 * @author Tfoedy
 * @brief Low-level rendering backend interface.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/slot_map.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/strong_handle.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

struct BufferTag {};
using BufferHandle = StrongHandle<BufferTag>;

struct TextureTag {};
using TextureHandle = StrongHandle<TextureTag>;

struct ShaderTag {};
using ShaderHandle = StrongHandle<ShaderTag>;

struct PipelineTag {};
using RenderPipelineHandle = StrongHandle<PipelineTag>;

/**
 * @brief Runtime GPU texture format.
 */
enum class TextureFormat : U8 {
    R8G8B8A8_UNorm,
    R8G8B8A8_SRGB,

    R16G16_Float,
    R16G16B16A16_Float,
    R32G32B32A32_Float,

    Depth24_Stencil8,
    Depth32_Float,
    Depth32_Float_Shadow
};

/**
 * @brief Texture dimensionality.
 */
enum class TextureDimension : U8 { Texture2D, Cube };

/**
 * @brief Texture creation descriptor.
 */
struct TextureDesc {
    U32 width{0};
    U32 height{0};
    U32 mip_levels{1};

    TextureFormat format{TextureFormat::R8G8B8A8_UNorm};
    TextureDimension dimension{TextureDimension::Texture2D};

    Slice<const Byte> initial_data{};
};

/**
 * @brief Face culling mode.
 */
enum class CullMode : U8 { None, Front, Back };

/**
 * @brief Pipeline blend mode.
 */
enum class BlendMode : U8 { None, Alpha };

/**
 * @brief Render attachment load operation.
 */
enum class RenderLoadOp : U8 { Clear, Load };

/**
 * @brief GPU buffer usage hint.
 */
enum class BufferUsage : U8 { Static, Dynamic };

/**
 * @brief GPU buffer creation descriptor.
 */
struct BufferDesc {
    USize size{0};
    BufferUsage usage{BufferUsage::Static};
    Slice<const Byte> initial_data{};
};

/**
 * @brief Render pipeline state descriptor.
 */
struct RenderPipelineProperties {
    ShaderHandle shader{};

    CullMode cull_mode{CullMode::Back};

    bool depth_test{true};
    bool depth_write{true};
    bool wireframe{false};

    BlendMode blend_mode{BlendMode::None};
};

/**
 * @brief Texture subresource used as a render pass attachment.
 */
struct RenderAttachment {
    TextureHandle texture{};
    U32 layer{0};
    U32 mip_level{0};
    RenderLoadOp load_op{RenderLoadOp::Clear};

    [[nodiscard]] bool is_valid() const noexcept {
        return texture.is_valid();
    }
};

/**
 * @brief Backend-agnostic command recording interface.
 */
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;

    /**
     * @brief Begins a render pass with whole-texture attachments.
     */
    virtual void begin_render_pass(Slice<const TextureHandle> color_targets,
                                   TextureHandle depth_target) noexcept = 0;

    /**
     * @brief Begins a render pass with explicit texture subresources.
     */
    virtual void begin_render_pass_ex(Slice<const RenderAttachment> color_targets,
                                      RenderAttachment depth_target) noexcept = 0;

    /**
     * @brief Ends the current render pass.
     */
    virtual void end_render_pass() noexcept = 0;

    /**
     * @brief Sets the active viewport.
     */
    virtual void set_viewport(U32 x, U32 y, U32 width, U32 height) noexcept = 0;

    /**
     * @brief Binds a render pipeline.
     */
    virtual void set_pipeline(RenderPipelineHandle pipeline) noexcept = 0;

    /**
     * @brief Binds a vertex buffer.
     */
    virtual void bind_vertex_buffer(BufferHandle vbo, U32 stride) noexcept = 0;

    /**
     * @brief Binds an index buffer.
     */
    virtual void bind_index_buffer(BufferHandle ibo) noexcept = 0;

    /**
     * @brief Binds a texture to a shader slot.
     */
    virtual void bind_texture(TextureHandle texture, U32 slot) noexcept = 0;

    /**
     * @brief Binds a shader storage buffer.
     */
    virtual void bind_storage_buffer(BufferHandle buffer, U32 slot) noexcept = 0;

    /**
     * @brief Sets four 32-bit push constant slots.
     *
     * @details
     * Current renderer layout:
     * - slot 0: transform index
     * - slot 1: material index
     * - slot 2: cascade index / cubemap face
     * - slot 3: shadow record index / mip index
     */
    virtual void set_push_constants(Slice<const Byte> data) noexcept = 0;

    /**
     * @brief Records an indexed draw.
     */
    virtual void draw_indexed(U32 index_count, U32 index_offset = 0,
                              U32 vertex_offset = 0) noexcept = 0;

    /**
     * @brief Records a non-indexed draw.
     */
    virtual void draw_arrays(U32 vertex_count, U32 first_vertex = 0) noexcept = 0;
};

/**
 * @brief Low-level renderer backend.
 */
class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    /**
     * @brief Creates a GPU buffer.
     */
    virtual BufferHandle create_buffer(const BufferDesc &desc) noexcept = 0;

    /**
     * @brief Creates a GPU buffer initialized with data.
     */
    BufferHandle create_buffer(Slice<const Byte> data, bool is_dynamic) noexcept {
        BufferDesc desc{};
        desc.size = data.size();
        desc.usage = is_dynamic ? BufferUsage::Dynamic : BufferUsage::Static;
        desc.initial_data = data;

        return create_buffer(desc);
    }

    /**
     * @brief Creates an uninitialized GPU buffer.
     */
    BufferHandle create_empty_buffer(USize size, bool is_dynamic) noexcept {
        BufferDesc desc{};
        desc.size = size;
        desc.usage = is_dynamic ? BufferUsage::Dynamic : BufferUsage::Static;

        return create_buffer(desc);
    }

    /**
     * @brief Updates a buffer region.
     */
    virtual void update_buffer(BufferHandle handle, Slice<const Byte> data,
                               U32 offset = 0) noexcept = 0;

    /**
     * @brief Creates a texture from a descriptor.
     */
    virtual TextureHandle create_texture(const TextureDesc &desc) noexcept = 0;

    /**
     * @brief Creates a 2D texture.
     */
    TextureHandle create_texture_2d(U32 width, U32 height, U32 mip_levels, TextureFormat format,
                                    Slice<const Byte> data = {}) noexcept {
        TextureDesc desc{};
        desc.width = width;
        desc.height = height;
        desc.mip_levels = mip_levels;
        desc.format = format;
        desc.dimension = TextureDimension::Texture2D;
        desc.initial_data = data;

        return create_texture(desc);
    }

    /**
     * @brief Creates a cubemap texture.
     */
    TextureHandle create_texture_cube(U32 size, U32 mip_levels, TextureFormat format) noexcept {
        TextureDesc desc{};
        desc.width = size;
        desc.height = size;
        desc.mip_levels = mip_levels;
        desc.format = format;
        desc.dimension = TextureDimension::Cube;

        return create_texture(desc);
    }

    /**
     * @brief Creates a shader program.
     */
    virtual ShaderHandle create_shader(StringView vertex_src, StringView fragment_src,
                                       StringView debug_name = {}) noexcept = 0;
    /**
     * @brief Creates a render pipeline.
     */
    virtual RenderPipelineHandle
    create_render_pipeline(const RenderPipelineProperties &properties) noexcept = 0;

    /**
     * @brief Destroys a GPU buffer.
     */
    virtual void destroy_buffer(BufferHandle handle) noexcept = 0;

    /**
     * @brief Destroys a GPU texture.
     */
    virtual void destroy_texture(TextureHandle handle) noexcept = 0;

    /**
     * @brief Destroys a shader program.
     */
    virtual void destroy_shader(ShaderHandle handle) noexcept = 0;

    /**
     * @brief Destroys a render pipeline.
     */
    virtual void destroy_pipeline(RenderPipelineHandle handle) noexcept = 0;

    /**
     * @brief Returns a command buffer for recording.
     */
    virtual CommandBuffer *adopt_command_buffer() noexcept = 0;

    /**
     * @brief Submits a recorded command buffer.
     */
    virtual void submit_command_buffer(CommandBuffer *cmd_buffer) noexcept = 0;

    /**
     * @brief Returns a backend-native texture handle.
     */
    virtual void *native_texture_handle(TextureHandle handle) noexcept = 0;
};

FR_API RenderDevice *create_opengl_render_device(Alloc *alloc) noexcept;

FR_API void destroy_opengl_render_device(RenderDevice *device) noexcept;

} // namespace fr
