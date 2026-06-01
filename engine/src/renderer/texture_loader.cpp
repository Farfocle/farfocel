// THIS WILL BE REMOVED FROM THE ENGINE

#include "fr/renderer/texture_loader.hpp"
#include "fr/core/string.hpp"
#include <stb_image.h>

namespace fr {

FR_API TextureHandle load_texture_2d(RenderDevice *device, StringView file_path,
                                     bool is_srgb) noexcept {
    String path = String::from_view(file_path);
    int width, height, channels;

    stbi_set_flip_vertically_on_load(false);

    stbi_uc *pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (!pixels) {
        return TextureHandle{};
    }

    Slice<const Byte> data_slice(reinterpret_cast<const Byte *>(pixels), width * height * 4);

    TextureFormat format = is_srgb ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNorm;

    TextureHandle handle = device->create_texture_2d(static_cast<U32>(width),
                                                     static_cast<U32>(height), format, data_slice);

    stbi_image_free(pixels);

    return handle;
}

} // namespace fr
