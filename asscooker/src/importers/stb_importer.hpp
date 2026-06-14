/**
 * @file stb_importer.hpp
 * @author Tfoedy
 * @brief STB texture importer.
 */

#pragma once

#include "../formats/raw_texture.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {

/**
 * @brief Imports an image file into RawTexture.
 */
bool import_texture(StringView input_path, RawTexture &out_texture, bool is_srgb) noexcept;

} // namespace fr::asscooker
