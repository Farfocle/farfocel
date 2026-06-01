/**
 * @file render_queue.hpp
 * @author Tfoedy
 *
 * @brief Collects and sorts draw calls before they are seent to the GPU.
 */
#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_device.hpp"

#include <algorithm>
#include <glm/glm.hpp>

namespace fr {

/**
 * @brief Determines the general category of the render pass for sorting purposes.
 */
enum class RenderPassType : U8 {
    /// Regular solid models
    Opaque = 0, // regular models
    /// Alpha-tested models (vegetation)
    Masked = 1, // leaves, vegetation
    /// Background skybox
    Skybox = 2,
    /// Semi-transparent that require blending
    Transparent = 3, // glass
    /// two dim
    TwoDim = 4, // 2D sprites
    UI = 5,
};

/**
 * @brief 64-bit key for optimal sorting, used to minimize GPU state changes.
 */
struct SortKey {
    U64 value{0};

    /**
     * @brief Creates a 64-bit sorting key based on bit hierarchy.
     * * @param pass Render pass type.
     * @param pipe Handle to the render pipeline.
     * @param texture Handle to the main texture.
     * @param depth Depth value for Z-sorting.
     * @return A composed SortKey.
     */
    static constexpr SortKey create(RenderPassType pass, RenderPipelineHandle pipe,
                                    TextureHandle texture, U32 depth) noexcept {
        // BIT HIERARCHY
        // [56-63] pass type
        // [40-55] pipeline ID
        // [24-39] texture ID
        // [0-23] depth

        U64 pass_bits = (static_cast<U64>(pass) & 0xFF) << 56;
        U64 pipe_bits = (static_cast<U64>(pipe.key.index) & 0xFFFF) << 40;
        U64 tex_bits = (static_cast<U64>(texture.key.index) & 0xFFFF) << 24;
        U64 dep_bits = static_cast<U64>(depth) & 0xFFFFFF;

        return SortKey{pass_bits | pipe_bits | tex_bits | dep_bits};
    }

    constexpr bool operator<(const SortKey &other) const noexcept {
        return value < other.value; // super duper fast
    }
};

/**
 * @brief Drawing call packet sent from the ECS to the RENDERER.
 */
struct alignas(8) DrawCall {
    /// Sorting key dictating the render execution order
    SortKey key; // 8 bytes

    /// Graphics pipeline properties
    RenderPipelineHandle pipe; // 4

    /// Vertex buffer handle
    BufferHandle vbo; // 4
    /// Index buffer handle
    BufferHandle ibo; // 4

    /// PBR albedo or base color map
    TextureHandle albedo_map; // 4
    /// Tangent space normal map
    TextureHandle normal_map; // 4
    /// Metallic-roughness mapr for PBR or specular map for Standard lighting model
    TextureHandle extra_map; // 4

    /// Amount of indices to draw
    U32 index_count; // 4
    /// Starting offset in the IBO
    U32 index_offset; // 4

    /// Base offset added to each index
    U32 vertex_offset; // 4
    /// Size of a single vertex
    U32 vbo_stride; // 4

    /// Index locating the model matrix in the global SSBO of transform matrices
    U32 transform_index; // 4 bytes

    /// ID specifying the shading model (0 = Unlit, 1 = Standard Blinn-Phpong, 2 = PBR)
    U32 shading_model; // 4 bytes

    /// @brief Less-than operator
    constexpr bool operator<(const DrawCall &other) const noexcept {
        return key < other.key;
    }
};

FR_STATIC_ASSERT(sizeof(DrawCall) == 80, "DrawCall must be exactly 80 bytes of size");

/// @brief GPU representation of a point light
struct alignas(16) PointLightData {
    glm::vec3 position;
    F32 radius;
    glm::vec3 color;
    F32 intensity;
};

struct alignas(16) DirectionalLightData {
    glm::vec3 direction;
    F32 intensity;
    glm::vec3 color;
    F32 padding; // for the 32 bytes of the std140 layout in the shaders
    glm::mat4 light_view_proj;
};

class RenderQueue {
public:
    /**
     * @brief Constructs a new RenderQueue.
     * * @param alloc Allocator used for the internal dynamic arrays.
     */
    explicit RenderQueue(Alloc *alloc = get_ambient_ctx().alloc) noexcept
        : m_alloc(alloc),
          m_packets(m_alloc),
          m_transforms(m_alloc),
          m_point_lights(m_alloc) {
        m_packets.reserve(4096);
        m_transforms.reserve(4096);
        m_point_lights.reserve(256);
    }

    RenderQueue(const RenderQueue &) = delete;
    RenderQueue &operator=(const RenderQueue &) = delete;
    RenderQueue(RenderQueue &&) noexcept = default;
    RenderQueue &operator=(RenderQueue &&) noexcept = default;

    /**
     * @brief Clears the accumulated data from the last frame.
     */
    void clear_leftover() noexcept {
        m_packets.clear();
        m_transforms.clear();
        m_point_lights.clear();
        m_directional_lights.clear();
    }

    /**
     * @brief Submits a draw call alongside its world transformation matrix.
     * * @param packet The draw call configuration.
     * @param transform The world space transformation matrix.
     */
    void send_call(const DrawCall &packet, const glm::mat4 &transform) noexcept {
        U32 transform_index = static_cast<U32>(m_transforms.size());
        m_transforms.push_back(transform);

        DrawCall p = packet;
        p.transform_index = transform_index;
        m_packets.push_back(p);
    }

    /// @brief Submits a point light to the current frame
    void send_point_light(const PointLightData &light) noexcept {
        m_point_lights.push_back(light);
    }

    /// @brief Retrieves a slice of the queued point lights of the frame
    Slice<const PointLightData> get_point_lights() const noexcept {
        return m_point_lights.slice();
    }

    /// @brief Submits a point light to the current frame
    void send_directional_light(const DirectionalLightData &light) noexcept {
        m_directional_lights.push_back(light);
    }

    /// @brief Retrieves a slice of the queued point lights of the frame
    Slice<const DirectionalLightData> get_directional_lights() const noexcept {
        return m_directional_lights.slice();
    }
    /**
     * @brief Sorts the queued draw calls to minimize state changes.
     * @note Future enhancement: Radix sort.
     */
    void sort() noexcept {
        std::sort(m_packets.begin(), m_packets.end());
    }

    /**
     * @brief Retrieves a read-only slice of the queued draw calls.
     * * @return Slice of DrawCall objects.
     */
    Slice<const DrawCall> get_calls() const noexcept {
        return m_packets.slice();
    }
    /**
     * @brief Retrieves a read-only slice of the transform matrices.
     * * @return Slice of glm::mat4 objects.
     */
    Slice<const glm::mat4> get_transforms() const noexcept {
        return m_transforms.slice();
    }
    /**
     * @brief Checks if the render queue is empty.
     * * @return true if no draw calls are queued.
     */
    bool is_empty() const noexcept {
        return m_packets.is_empty();
    }

private:
    Alloc *m_alloc;
    DynamicArray<DrawCall> m_packets;
    DynamicArray<glm::mat4> m_transforms;
    DynamicArray<PointLightData> m_point_lights;
    DynamicArray<DirectionalLightData> m_directional_lights;
};

} // namespace fr
