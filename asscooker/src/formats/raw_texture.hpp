#pragma once
#include "fr/core/dynamic_array.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/asset_format.hpp"

namespace fr::asscooker {

struct RawTexture {
    U32 width{0};
    U32 height{0};
    U32 channels{0};
    U32 pixel_size{0};
    U32 mip_levels{1};
    AssetTextureFormat format{AssetTextureFormat::RGBA8_UNORM};
    DynamicArray<U8> pixel_data;
};

} // namespace fr::asscooker
