/**
 * @file pack_compiler.hpp
 * @author Tfoedy
 * @brief Cooked asset pack compiler.
 */

#pragma once

#include "fr/asscooker/asscooker.hpp"

namespace fr::asscooker {

/**
 * @brief Writes cooked assets into a .fpack file.
 */
bool compile_pack(Slice<const PackAssetInput> assets, StringView output_path) noexcept;

} // namespace fr::asscooker
