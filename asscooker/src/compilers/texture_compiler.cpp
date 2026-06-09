#include "texture_compiler.hpp"
#include "fr/core/string.hpp"
#include "fr/data/asset_format.hpp"
#include <cstdio>
#include <fstream>

namespace fr::asscooker {

/**
 * @brief Serializes a RawTexture into the cooked `.ftex` binary format.
 *
 * @details
 * The resulting file layout is:
 *
 * - TextureHeader
 * - raw image payload for the base mip level
 *
 * The current runtime generates mipmaps on the GPU, so the file stores only the base image data.
 * All write operations are validated.
 *
 * @param raw Source texture in the intermediate cooker format.
 * @param output_path Destination path for the cooked `.ftex` file.
 * @return True when the texture was written successfully, false otherwise.
 */
bool compile_texture(const RawTexture &raw, StringView output_path) {
    String out_path_str = String::from_view(output_path);

    std::ofstream file(out_path_str.data(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    auto write_items = [&](const void *data, USize item_size, USize item_count) -> bool {
        if (item_count == 0) {
            return true;
        }

        if (!data || item_size == 0) {
            return false;
        }

        file.write(static_cast<const char *>(data),
                   static_cast<std::streamsize>(item_size * item_count));

        return static_cast<bool>(file);
    };

    if (raw.width == 0 || raw.height == 0 || raw.mip_levels == 0 || raw.pixel_data.is_empty()) {
        return false;
    }

    TextureHeader header{};
    header.base.verify[0] = 'F';
    header.base.verify[1] = 'T';
    header.base.verify[2] = 'E';
    header.base.verify[3] = 'X';
    header.base.version = 1;

    header.width = raw.width;
    header.height = raw.height;
    header.format = raw.format;
    header.mip_levels = raw.mip_levels;
    header.image_data_size = static_cast<U32>(raw.pixel_data.size());

    if (!write_items(&header, sizeof(TextureHeader), 1)) {
        return false;
    }

    if (!write_items(raw.pixel_data.data(), 1, raw.pixel_data.size())) {
        return false;
    }

    file.close();
    return static_cast<bool>(file);
}

} // namespace fr::asscooker
