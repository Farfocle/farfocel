/**
 * @file asscooker.cpp
 * @author Tfoedy
 * @brief Asset cooker public API implementation.
 */

#include "fr/asscooker/asscooker.hpp"

#include <utility>

#include "compilers/manifest_compiler.hpp"
#include "compilers/material_compiler.hpp"
#include "compilers/mesh_compiler.hpp"
#include "compilers/pack_compiler.hpp"
#include "compilers/shader_compiler.hpp"
#include "compilers/texture_compiler.hpp"
#include "formats/raw_material.hpp"
#include "formats/raw_shader.hpp"
#include "importers/gltf_importer.hpp"
#include "importers/stb_importer.hpp"

#include "fr/core/ctx.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {
namespace {

template <typename T, USize N>
void copy_array(const T (&src)[N], T (&dst)[N]) noexcept {
    for (USize i = 0; i < N; ++i) {
        dst[i] = src[i];
    }
}

[[nodiscard]] bool read_text_file(StringView path, String &out_text) noexcept {
    if (path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot read text file from empty path.");
        return false;
    }

    String path_string = String::from_view(path);

    auto text = file::read_all_text(path_string);
    if (!text.is_some()) {
        FR_LOG_ERR("[Cooker] Failed to read text file: {}", path);
        return false;
    }

    out_text = std::move(text.unwrap());

    if (out_text.size() == 0) {
        FR_LOG_ERR("[Cooker] Text file is empty: {}", path);
        return false;
    }

    return true;
}

[[nodiscard]] RawMaterial make_raw_material(const MaterialCookDesc &desc) noexcept {
    RawMaterial raw{};
    raw.albedo_texture = desc.albedo_texture;
    raw.normal_texture = desc.normal_texture;
    raw.extra_texture = desc.extra_texture;

    copy_array(desc.base_color_factor, raw.base_color_factor);

    raw.metallic_factor = desc.metallic_factor;
    raw.roughness_factor = desc.roughness_factor;
    raw.alpha = desc.alpha;
    raw.alpha_cutoff = desc.alpha_cutoff;

    raw.shading_model = desc.shading_model;
    raw.blend_mode = desc.blend_mode;

    return raw;
}

[[nodiscard]] bool has_cooked_asset_output(const DynamicArray<CookedAssetOutput> &outputs,
                                           AssetId id) noexcept {
    if (!id.is_valid()) {
        return false;
    }

    for (USize i = 0; i < outputs.size(); ++i) {
        if (outputs[i].id == id) {
            return true;
        }
    }

    return false;
}

void append_cooked_asset_output_once(DynamicArray<CookedAssetOutput> *outputs, AssetId id,
                                     AssetKind kind, StringView path,
                                     U64 content_hash = 0) noexcept {
    if (!outputs || has_cooked_asset_output(*outputs, id)) {
        return;
    }

    append_cooked_asset_output(outputs, id, kind, path, content_hash);
}

} // namespace

void append_cooked_asset_output(DynamicArray<CookedAssetOutput> *outputs, AssetId id,
                                AssetKind kind, StringView path, U64 content_hash) noexcept {
    if (!outputs) {
        return;
    }

    if (!id.is_valid() || kind == AssetKind::Unknown || path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot append invalid cooked asset output.");
        return;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    CookedAssetOutput output{};
    output.id = id;
    output.kind = kind;
    output.path = String::from_view(alloc, path);
    output.content_hash = content_hash;

    outputs->push_back(std::move(output));
}

AssetId resolve_output_asset_id(StringView cooked_path, const CookOptions &options) noexcept {
    if (options.output_id.is_valid()) {
        return options.output_id;
    }

    if (cooked_path.is_empty()) {
        return {};
    }

    return AssetId::from_logical_path(cooked_path);
}

bool cook_mesh(StringView input_path, StringView output_path) {
    return cook_mesh_ex(input_path, output_path, nullptr);
}

bool cook_mesh_ex(StringView input_path, StringView output_path,
                  DynamicArray<CookedAssetOutput> *outputs, CookOptions options) noexcept {
    if (input_path.is_empty() || output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot cook mesh with empty path.");
        return false;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    CookOptions import_options = options;

    String generated_asset_dir;
    if (import_options.generated_asset_dir.is_empty()) {
        generated_asset_dir = String::from_view(file::get_parent_path(output_path));
        file::normalize_unix(generated_asset_dir);
        import_options.generated_asset_dir = generated_asset_dir.view();
    }

    RawMesh raw(alloc);
    if (!import_gltf(input_path, raw, outputs, import_options)) {
        FR_LOG_ERR("[Cooker] Failed to import glTF mesh: {}", input_path);
        return false;
    }

    if (!compile_mesh(raw, output_path)) {
        FR_LOG_ERR("[Cooker] Failed to compile cooked mesh: {}", output_path);
        return false;
    }

    const AssetId output_id = resolve_output_asset_id(output_path, options);
    append_cooked_asset_output_once(outputs, output_id, AssetKind::Mesh, output_path);

    return true;
}

bool cook_texture(StringView input_path, StringView output_path, bool is_srgb) {
    return cook_texture_ex(input_path, output_path, is_srgb, nullptr);
}

bool cook_texture_ex(StringView input_path, StringView output_path, bool is_srgb,
                     DynamicArray<CookedAssetOutput> *outputs, CookOptions options) noexcept {
    if (input_path.is_empty() || output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot cook texture with empty path.");
        return false;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    RawTexture raw(alloc);
    if (!import_texture(input_path, raw, is_srgb)) {
        FR_LOG_ERR("[Cooker] Failed to import texture: {}", input_path);
        return false;
    }

    if (!compile_texture(raw, output_path)) {
        FR_LOG_ERR("[Cooker] Failed to compile cooked texture: {}", output_path);
        return false;
    }

    const AssetId output_id = resolve_output_asset_id(output_path, options);
    append_cooked_asset_output_once(outputs, output_id, AssetKind::Texture, output_path);

    return true;
}

bool cook_shader(StringView vertex_path, StringView fragment_path, StringView output_path) {
    return cook_shader_ex(vertex_path, fragment_path, output_path, nullptr);
}

bool cook_shader_ex(StringView vertex_path, StringView fragment_path, StringView output_path,
                    DynamicArray<CookedAssetOutput> *outputs, CookOptions options) noexcept {
    if (vertex_path.is_empty() || fragment_path.is_empty() || output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot cook shader with empty path.");
        return false;
    }

    String vertex_source{};
    String fragment_source{};

    if (!read_text_file(vertex_path, vertex_source)) {
        FR_LOG_ERR("[Cooker] Failed to read vertex shader: {}", vertex_path);
        return false;
    }

    if (!read_text_file(fragment_path, fragment_source)) {
        FR_LOG_ERR("[Cooker] Failed to read fragment shader: {}", fragment_path);
        return false;
    }

    RawShader shader{};
    shader.stages.reserve(2);

    RawShaderStage vertex_stage{};
    vertex_stage.stage = CookedShaderStage::Vertex;
    vertex_stage.source = std::move(vertex_source);
    shader.stages.push_back(std::move(vertex_stage));

    RawShaderStage fragment_stage{};
    fragment_stage.stage = CookedShaderStage::Fragment;
    fragment_stage.source = std::move(fragment_source);
    shader.stages.push_back(std::move(fragment_stage));

    if (!compile_shader(shader, output_path)) {
        FR_LOG_ERR("[Cooker] Failed to compile cooked shader: {}", output_path);
        return false;
    }

    const AssetId output_id = resolve_output_asset_id(output_path, options);
    append_cooked_asset_output_once(outputs, output_id, AssetKind::Shader, output_path);

    return true;
}

bool cook_material(const MaterialCookDesc &desc, StringView output_path) {
    return cook_material_ex(desc, output_path, nullptr);
}

bool cook_material_ex(const MaterialCookDesc &desc, StringView output_path,
                      DynamicArray<CookedAssetOutput> *outputs, CookOptions options) noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot cook material with empty output path.");
        return false;
    }

    RawMaterial raw = make_raw_material(desc);

    if (!compile_material(raw, output_path)) {
        FR_LOG_ERR("[Cooker] Failed to compile cooked material: {}", output_path);
        return false;
    }

    const AssetId output_id = resolve_output_asset_id(output_path, options);
    append_cooked_asset_output_once(outputs, output_id, AssetKind::Material, output_path);

    return true;
}

bool build_pack(Slice<const PackAssetInput> assets, StringView output_path) {
    return compile_pack(assets, output_path);
}

bool build_manifest(const ManifestBuildDesc &desc, StringView output_path) {
    return compile_manifest(desc, output_path);
}

} // namespace fr::asscooker
