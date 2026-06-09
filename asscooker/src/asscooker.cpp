/**
 * @file asscooker.cpp
 * @brief Asset cooker.
 */

#include "fr/asscooker/asscooker.hpp"
#include "compilers/mesh_compiler.hpp"
#include "compilers/texture_compiler.hpp"
#include "importers/gltf_importer.hpp"
#include "importers/stb_importer.hpp"

namespace fr::asscooker {

bool cook_mesh(StringView input_path, StringView output_path) {
    RawMesh raw;
    if (!import_gltf(input_path, raw)) {
        return false;
    }
    return compile_mesh(raw, output_path);
}

bool cook_texture(StringView input_path, StringView output_path, bool is_srgb) {
    RawTexture raw;
    if (!import_texture(input_path, raw, is_srgb)) {
        return false;
    }
    return compile_texture(raw, output_path);
}

} // namespace fr::asscooker
