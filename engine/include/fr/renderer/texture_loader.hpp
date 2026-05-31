/**
 *
 * @file texture_loader.hpp
 * @author Tfoedy
 * @brief Loads a 2D texture file into the VRAM
 */

#pragma once

#include "fr/core/string_view.hpp"
#include "fr/renderer/render_device.hpp"

/**
 * @brief Loads an image file from disk and uploads it directly to the GPU.
 * @param device Pointer to the active hardware render device.
 * @param file_path Path to the image file (e.g., PNG, JPG).
 * @param is_srgb True if the texture represents color (Albedo), enforcing sRGB color space.
 * @return Safe strong handle to the allocated texture. Empty handle if loading fails.
 */
namespace fr {
FR_API TextureHandle load_texture_2d(RenderDevice *device, StringView path, bool is_srgb) noexcept;

}
