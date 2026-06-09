/**
 * @file stb_importer.hpp
 * @brief Declarations for the STB-based image decoding pipeline.
 */
#pragma once

#include "../formats/raw_texture.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {

/**
 * @brief Imports raw pixels from a file and prepares them in a RawTexture structure.
 * @param input_path Path to the source image file.
 * @param out_texture Reference to the structure where decoded pixels and format flags will be
 * stored.
 * @param is_srgb Explicit flag defining the color space semantic (True for Albedo, False for data
 * maps).
 * @return True on successful decode.
 */
bool import_texture(StringView input_path, RawTexture &out_texture, bool is_srgb);

} // namespace fr::asscooker
