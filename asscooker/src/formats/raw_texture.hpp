/**
 * @file raw_texture.hpp
 * @author Tfoedy
 * @brief Intermediate texture data used by the asset cooker.
 */

#pragma once

#include "fr/core/dynamic_array.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_format.hpp"

namespace fr::asscooker {

/**
 * @brief Texture data ready for .ftex compilation.
 */
struct RawTexture {
    U32 width{0};
    U32 height{0};
    U32 channels{0};
    U32 bytes_per_pixel{0};
    U32 mip_levels{1};

    CookedTextureFormat format{CookedTextureFormat::RGBA8_UNORM};

    DynamicArray<U8> pixels;

    explicit RawTexture(Alloc *alloc) noexcept
        : pixels(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");
    }
};

} // namespace fr::asscooker
