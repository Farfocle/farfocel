/**
 * @file gltf_importer.hpp
 * @author Tfoedy
 * @brief glTF mesh importer.
 */

#pragma once

#include "formats/raw_mesh.hpp"

#include "fr/asscooker/asscooker.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {

/**
 * @brief Imports a glTF file into RawMesh.
 */
bool import_gltf(StringView input_path, RawMesh &out_mesh,
                 DynamicArray<CookedAssetOutput> *outputs = nullptr,
                 CookOptions options = {}) noexcept;

} // namespace fr::asscooker
