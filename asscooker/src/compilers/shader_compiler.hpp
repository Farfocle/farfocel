/**
 * @file shader_compiler.hpp
 * @author Tfoedy
 * @brief Shader asset compiler.
 */

#pragma once

#include "formats/raw_shader.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {

/**
 * @brief Writes a raw shader into .fshader.
 */
bool compile_shader(const RawShader &shader, StringView output_path) noexcept;

} // namespace fr::asscooker
