/**
 * @file render_sort_key.hpp
 * @author Tfoedy
 * @brief Render pass classification and draw sort keys.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_device.hpp"

namespace fr {

/**
 * @brief Material/render path classification used by extraction.
 */
enum class RenderPass : U8 {
    Opaque = 0,
    Masked = 1,

    /**
     * @brief Transparent material path.
     *
     * @note The current deferred renderer skips this pass until a forward transparent pass exists.
     */
    Transparent = 2,
};

/**
 * @brief Packed key used to sort draw calls inside one draw list.
 */
struct RenderSortKey {
    U64 value{0};

    /**
     * @brief Builds a state-oriented draw sort key.
     */
    static constexpr RenderSortKey create(RenderPipelineHandle pipe, BufferHandle vbo,
                                          TextureHandle texture, U32 depth = 0) noexcept {
        const U64 pipe_bits = (static_cast<U64>(pipe.key.index) & 0xFFFULL) << 52;
        const U64 vbo_bits = (static_cast<U64>(vbo.key.index) & 0xFFFFULL) << 36;
        const U64 tex_bits = (static_cast<U64>(texture.key.index) & 0xFFFFULL) << 20;
        const U64 depth_bits = static_cast<U64>(depth) & 0xFFFFFULL;

        return RenderSortKey{pipe_bits | vbo_bits | tex_bits | depth_bits};
    }

    constexpr bool operator<(const RenderSortKey &other) const noexcept {
        return value < other.value;
    }
};

} // namespace fr
