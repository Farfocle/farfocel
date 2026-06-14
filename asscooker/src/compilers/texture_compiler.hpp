/**
 * @file texture_compiler.hpp
 * @author Tfoedy
 * @brief Texture asset compiler.
 */

#pragma once

#include "formats/raw_texture.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {

/**
 * @brief Writes a raw texture into .ftex.
 */
bool compile_texture(const RawTexture &raw_texture, StringView output_path) noexcept;

} // namespace fr::asscooker
