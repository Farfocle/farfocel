/** @file stb_importer.cpp
 * @brief STB texture importer.
 */

#include "stb_importer.hpp"

#include "fr/core/math.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stb_image.h>

namespace fr::asscooker {
namespace {

/**
 * @brief Prints an image decode failure with filesystem and STB diagnostics.
 */
static void log_stbi_failure(StringView path, const char *kind) noexcept {
    String path_str = String::from_view(path);
    const bool exists = std::filesystem::exists(path_str.data());
    const char *reason = stbi_failure_reason();

    std::printf("[Cooker Error] Failed to decode %s texture.\n", kind ? kind : "image");
    std::printf("  path:   %s\n", path_str.data());
    std::printf("  exists: %s\n", exists ? "yes" : "no");
    std::printf("  stb:    %s\n", reason ? reason : "unknown");
}

static U32 mip_count_for_size(S32 width, S32 height) noexcept {
    S32 max_dim = fr::math::max(width, height);
    return 1 + static_cast<U32>(std::floor(std::log2(static_cast<F32>(max_dim))));
}

} // namespace

bool import_texture(StringView input_path, RawTexture &out_texture, bool is_srgb) {
    String path_str = String::from_view(input_path);

    S32 width = 0;
    S32 height = 0;
    S32 channels = 0;

    stbi_set_flip_vertically_on_load(false);

    const bool is_hdr = stbi_is_hdr(path_str.data()) != 0;

    if (is_hdr) {
        F32 *data = stbi_loadf(path_str.data(), &width, &height, &channels, 4);
        if (!data) {
            log_stbi_failure(input_path, "HDR");
            return false;
        }

        if (width <= 0 || height <= 0) {
            std::printf("[Cooker Error] Invalid HDR texture dimensions: %s (%d x %d)\n",
                        path_str.data(), width, height);
            stbi_image_free(data);
            return false;
        }

        out_texture.width = static_cast<U32>(width);
        out_texture.height = static_cast<U32>(height);
        out_texture.channels = 4;
        out_texture.pixel_size = sizeof(F32);
        out_texture.format = AssetTextureFormat::RGBA32F_HDR;
        out_texture.mip_levels = mip_count_for_size(width, height);

        const USize size = static_cast<USize>(width) * static_cast<USize>(height) * 4 * sizeof(F32);

        out_texture.pixel_data.clear();
        out_texture.pixel_data.grow_default(size);

        fr::mem::copy_raw_range(reinterpret_cast<const U8 *>(data), size,
                                out_texture.pixel_data.data());

        stbi_image_free(data);
        return true;
    }

    stbi_uc *data = stbi_load(path_str.data(), &width, &height, &channels, 4);
    if (!data) {
        log_stbi_failure(input_path, "LDR");
        return false;
    }

    if (width <= 0 || height <= 0) {
        std::printf("[Cooker Error] Invalid LDR texture dimensions: %s (%d x %d)\n",
                    path_str.data(), width, height);
        stbi_image_free(data);
        return false;
    }

    out_texture.width = static_cast<U32>(width);
    out_texture.height = static_cast<U32>(height);
    out_texture.channels = 4;
    out_texture.pixel_size = 1;
    out_texture.format = is_srgb ? AssetTextureFormat::RGBA8_SRGB : AssetTextureFormat::RGBA8_UNORM;
    out_texture.mip_levels = mip_count_for_size(width, height);

    const USize size = static_cast<USize>(width) * static_cast<USize>(height) * 4;

    out_texture.pixel_data.clear();
    out_texture.pixel_data.grow_default(size);

    fr::mem::copy_raw_range(reinterpret_cast<const U8 *>(data), size,
                            out_texture.pixel_data.data());

    stbi_image_free(data);
    return true;
}

} // namespace fr::asscooker
