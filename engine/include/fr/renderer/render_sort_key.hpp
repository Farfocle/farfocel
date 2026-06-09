/**
 * @file render_sort_key.hpp
 * @author Tfoedy
 * @brief Render pass classification and draw-call sorting data.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_device.hpp"

namespace fr {

enum class RenderPassType : U8 {
    Opaque = 0,
    Masked = 1,
    Skybox = 2,
    Transparent = 3,
    TwoDim = 4,
    UI = 5
};

/**
 * @brief Packed draw-call sort key.
 */
struct SortKey {
    U64 value{0};

    static constexpr SortKey create(RenderPassType pass, RenderPipelineHandle pipe,
                                    BufferHandle vbo, TextureHandle texture, U32 depth) noexcept {
        const U64 pass_bits = (static_cast<U64>(pass) & 0x7ULL) << 61;
        const U64 pipe_bits = (static_cast<U64>(pipe.key.index) & 0xFFFULL) << 49;
        const U64 vbo_bits = (static_cast<U64>(vbo.key.index) & 0xFFFFULL) << 33;
        const U64 tex_bits = (static_cast<U64>(texture.key.index) & 0x7FFFULL) << 18;
        const U64 depth_bits = static_cast<U64>(depth) & 0x3FFFFULL;

        return SortKey{pass_bits | pipe_bits | vbo_bits | tex_bits | depth_bits};
    }

    constexpr RenderPassType pass_type() const noexcept {
        return static_cast<RenderPassType>((value >> 61) & 0x7ULL);
    }

    constexpr bool operator<(const SortKey &other) const noexcept {
        return value < other.value;
    }
};

/**
 * @brief Renderable indexed draw call.
 */
struct alignas(8) DrawCall {
    SortKey key;

    RenderPipelineHandle pipe{};
    BufferHandle vbo{};
    BufferHandle ibo{};

    TextureHandle albedo_map{};
    TextureHandle normal_map{};
    TextureHandle extra_map{};

    U32 index_count{0};
    U32 index_offset{0};
    U32 vertex_offset{0};
    U32 vbo_stride{0};

    U32 transform_index{0};
    U32 shading_model{0};

    F32 alpha{1.0f};
    F32 sort_depth{0.0f};

    constexpr bool operator<(const DrawCall &other) const noexcept {
        return key < other.key;
    }
};

} // namespace fr
