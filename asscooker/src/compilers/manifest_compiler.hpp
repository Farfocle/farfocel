/**
 * @file manifest_compiler.hpp
 * @author Tfoedy
 * @brief Runtime asset manifest compiler.
 */

#pragma once

#include "fr/asscooker/asscooker.hpp"

namespace fr::asscooker {

/**
 * @brief Writes manifest input into a .fmanifest file.
 */
bool compile_manifest(const ManifestBuildDesc &desc, StringView output_path) noexcept;

} // namespace fr::asscooker
