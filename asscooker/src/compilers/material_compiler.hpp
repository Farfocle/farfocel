/**
 * @file material_compiler.hpp
 * @author Tfoedy
 * @brief Material asset compiler.
 */

#pragma once

#include "formats/raw_material.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {

/**
 * @brief Writes a raw material into .fmat.
 */
bool compile_material(const RawMaterial &material, StringView output_path) noexcept;

} // namespace fr::asscooker
