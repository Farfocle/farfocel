/**
 * @file stb_importer.cpp
 * @author Tfoedy
 * @brief STB texture importer.
 */

#include "stb_importer.hpp"

#include "fr/core/math.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"

#include <stb_image.h>

namespace fr::asscooker {
namespace {

void log_stbi_failure(StringView path, const char *kind) noexcept {
    const char *reason = stbi_failure_reason();

    FR_LOG_ERR("[Cooker] Failed to decode {} texture: {}. STB reason: {}.", kind ? kind : "image",
               path, reason ? reason : "unknown");
}

U32 mip_count_for_size(S32 width, S32 height) noexcept {
    S32 max_dim = fr::math::max(width, height);
    U32 mip_count = 1;

    while (max_dim > 1) {
        max_dim /= 2;
        ++mip_count;
    }

    return mip_count;
}

} // namespace

bool import_texture(StringView input_path, RawTexture &out_texture, bool is_srgb) noexcept {
    if (input_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot import texture from empty path.");
        return false;
    }

    String path_str = String::from_view(input_path);

    S32 width = 0;
    S32 height = 0;
    S32 channels = 0;

    stbi_set_flip_vertically_on_load(false);

    const bool is_hdr = stbi_is_hdr(path_str.c_str()) != 0;

    if (is_hdr) {
        F32 *data = stbi_loadf(path_str.c_str(), &width, &height, &channels, 4);
        if (!data) {
            log_stbi_failure(input_path, "HDR");
            return false;
        }

        if (width <= 0 || height <= 0) {
            FR_LOG_ERR("[Cooker] Invalid HDR texture dimensions: {} ({}x{}).", input_path, width,
                       height);
            stbi_image_free(data);
            return false;
        }

        out_texture.width = static_cast<U32>(width);
        out_texture.height = static_cast<U32>(height);
        out_texture.channels = 4;
        out_texture.bytes_per_pixel = sizeof(F32) * 4;
        out_texture.format = CookedTextureFormat::RGBA32F_HDR;
        out_texture.mip_levels = mip_count_for_size(width, height);

        const USize size =
            static_cast<USize>(width) * static_cast<USize>(height) * out_texture.bytes_per_pixel;

        out_texture.pixels.clear();
        out_texture.pixels.grow_default(size);

        fr::mem::copy_raw_range(reinterpret_cast<const U8 *>(data), size,
                                out_texture.pixels.data());

        stbi_image_free(data);
        return true;
    }

    stbi_uc *data = stbi_load(path_str.c_str(), &width, &height, &channels, 4);
    if (!data) {
        log_stbi_failure(input_path, "LDR");
        return false;
    }

    if (width <= 0 || height <= 0) {
        FR_LOG_ERR("[Cooker] Invalid LDR texture dimensions: {} ({}x{}).", input_path, width,
                   height);
        stbi_image_free(data);
        return false;
    }

    out_texture.width = static_cast<U32>(width);
    out_texture.height = static_cast<U32>(height);
    out_texture.channels = 4;
    out_texture.bytes_per_pixel = 4;
    out_texture.format =
        is_srgb ? CookedTextureFormat::RGBA8_SRGB : CookedTextureFormat::RGBA8_UNORM;
    out_texture.mip_levels = mip_count_for_size(width, height);

    const USize size =
        static_cast<USize>(width) * static_cast<USize>(height) * out_texture.bytes_per_pixel;

    out_texture.pixels.clear();
    out_texture.pixels.grow_default(size);

    fr::mem::copy_raw_range(reinterpret_cast<const U8 *>(data), size, out_texture.pixels.data());

    stbi_image_free(data);
    return true;
}

} // namespace fr::asscooker
