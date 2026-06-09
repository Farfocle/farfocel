/**
 * @file asscooker.hpp
 * @author Tfoedy
 * @brief Public API for the Farfocel Asset Cooker.
 */
#pragma once

#include "fr/core/string_view.hpp"

namespace fr::asscooker {

/**
 * @brief Parses a GLTF model and bakes it into a highly optimized binary .fmesh file.
 * @param input_path Virtual path to the source .gltf file.
 * @param output_path Virtual path where the cooked .fmesh should be written.
 * @return True if the cooking process was successful, false otherwise.
 */
bool cook_mesh(StringView input_path, StringView output_path);

/**
 * @brief Bakes an image payload into a native .ftex binary file.
 * @param input_path Virtual path to the source image (PNG, JPG, HDR).
 * @param output_path Virtual path where the cooked .ftex should be written.
 * @param is_srgb True if the texture contains color data (Albedo/Diffuse). False for mathematical
 * data (Normals, Masks).
 * @return True if the cooking process was successful.
 */
bool cook_texture(StringView input_path, StringView output_path, bool is_srgb = false);

} // namespace fr::asscooker
