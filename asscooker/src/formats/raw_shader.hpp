/**
 * @file raw_shader.hpp
 * @author Tfoedy
 * @brief Intermediate shader data used by the asset cooker.
 */

#pragma once

#include "fr/asset/shader_format.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/string.hpp"

namespace fr::asscooker {

/**
 * @brief Shader source stage before .fshader compilation.
 */
struct RawShaderStage {
    CookedShaderStage stage{CookedShaderStage::Vertex};
    String source{};
};

/**
 * @brief Shader source bundle before .fshader compilation.
 */
struct RawShader {
    DynamicArray<RawShaderStage> stages;
};

} // namespace fr::asscooker
