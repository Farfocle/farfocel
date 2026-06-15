/**
 * @file dev_asset_importer.cpp
 * @author Tfoedy
 * @brief Development-side source asset import helpers.
 */

#include "fr/asscooker/dev_asset_importer.hpp"

#include "fr/asscooker/asscooker.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {
namespace {

static bool is_path_separator(char c) noexcept {
    return c == '/' || c == '\\';
}

/**
 * @brief Normalizes a path to a compact slash-separated representation.
 */
static String normalize_slash_path(StringView path) noexcept {
    String input = file::get_normalized_unix(path);
    String out = String::with_capacity(input.size());

    bool last_was_slash = false;

    for (USize i = 0; i < input.size(); ++i) {
        const char c = input[i];

        if (c == '/') {
            if (out.size() == 0) {
                out.push_back(c);
                last_was_slash = true;
                continue;
            }

            if (last_was_slash) {
                continue;
            }

            out.push_back(c);
            last_was_slash = true;
            continue;
        }

        out.push_back(c);
        last_was_slash = false;
    }

    while (out.starts_with("./")) {
        out.erase(0, 2);
    }

    while (out.size() > 1 && out.back() == '/') {
        out.shrink(out.size() - 1);
    }

    return out;
}

/**
 * @brief Appends one path component to an existing slash-normalized path.
 */
static void append_path_component(String &path, StringView component) noexcept {
    if (component.is_empty()) {
        return;
    }

    if (path.size() == 0) {
        path.append(component);
        return;
    }

    if (!is_path_separator(path.back())) {
        path.push_back('/');
    }

    path.append(component);
}

/**
 * @brief Joins two path components and normalizes the result.
 */
static String join_paths(StringView a, StringView b) noexcept {
    String out = String::with_capacity(a.size() + b.size() + 1);

    if (!a.is_empty()) {
        out.append(a);
    }

    append_path_component(out, b);

    return normalize_slash_path(out.view());
}

/**
 * @brief Joins four path components and normalizes the result.
 */
static String join_paths(StringView a, StringView b, StringView c, StringView d) noexcept {
    String out = String::with_capacity(a.size() + b.size() + c.size() + d.size() + 4);

    if (!a.is_empty()) {
        out.append(a);
    }

    append_path_component(out, b);
    append_path_component(out, c);
    append_path_component(out, d);

    return normalize_slash_path(out.view());
}

/**
 * @brief Replaces characters that are unsafe in generated file and directory names.
 */
static void sanitize_asset_name(String &name) noexcept {
    if (name.size() == 0) {
        name = String::from_chars("asset");
        return;
    }

    for (USize i = 0; i < name.size(); ++i) {
        char &c = name[i];

        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|' || c == '.') {
            c = '_';
        }
    }

    while (name.starts_with("_")) {
        name.erase(0, 1);
    }

    while (name.size() > 0 && name.back() == '_') {
        name.shrink(name.size() - 1);
    }

    if (name.size() == 0) {
        name = String::from_chars("asset");
    }
}

/**
 * @brief Builds import name from explicit name or source file stem.
 */
static String make_import_name(StringView source_path, StringView import_name) noexcept {
    String name;

    if (!import_name.is_empty()) {
        name = String::from_view(import_name);
    } else {
        name = String::from_view(file::get_stem(source_path));
    }

    sanitize_asset_name(name);
    return name;
}

/**
 * @brief Builds cooked .fmesh path for an imported model.
 */
static String make_imported_mesh_path(StringView cooked_root, StringView import_name) noexcept {
    String filename = String::with_capacity(import_name.size() + 8);
    filename.append(import_name);
    filename.append(".fmesh");

    return join_paths(cooked_root, "models/imported", import_name, filename.view());
}

/**
 * @brief Registers only the outputs produced by the current import.
 */
static bool register_import_outputs(AssetRegistry &registry,
                                    Slice<const CookedAssetOutput> outputs) noexcept {
    bool ok = true;

    for (const CookedAssetOutput &output : outputs) {
        if (!output.id.is_valid() || output.kind == AssetKind::Unknown || output.path.size() == 0) {
            FR_LOG_ERR("[Cooker] Invalid imported cooked asset output.");
            ok = false;
            continue;
        }

        ok = registry.register_loose_asset(output.id, output.kind, output.path.view()) && ok;
    }

    return ok;
}

} // namespace

ImportedModelResult import_gltf_model(DevAssetImportContext &ctx, StringView source_path,
                                      StringView import_name, bool force) noexcept {
    ImportedModelResult result{};

    if (!ctx.is_valid()) {
        FR_LOG_ERR("[Cooker] Cannot import glTF model with invalid DevAssetImportContext.");
        return result;
    }

    if (source_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot import glTF model from empty source path.");
        return result;
    }

    String normalized_source = normalize_slash_path(source_path);
    String source_file = String::from_view(normalized_source.view());

    if (!file::exists(source_file)) {
        FR_LOG_ERR("[Cooker] glTF source file does not exist: {}", normalized_source.view());
        return result;
    }

    String safe_import_name = make_import_name(normalized_source.view(), import_name);
    String cooked_mesh_path = make_imported_mesh_path(ctx.cooked_root, safe_import_name.view());

    DynamicArray<CookedAssetOutput> outputs(ctx.alloc);

    CookOptions options{};
    options.output_id = AssetId::from_logical_path(cooked_mesh_path.view());
    options.force = force;

    /*
        cook_mesh_ex() derives generated_asset_dir from the primary .fmesh output path when this
        option is empty. That keeps generated .fmat/.ftex files next to the .fmesh.
    */
    options.generated_asset_dir = {};

    if (!cook_mesh_ex(normalized_source.view(), cooked_mesh_path.view(), &outputs, options)) {
        FR_LOG_ERR("[Cooker] Failed to import glTF model: {}", normalized_source.view());
        return result;
    }

    if (outputs.is_empty()) {
        FR_LOG_ERR("[Cooker] glTF import produced no cooked outputs: {}", normalized_source.view());
        return result;
    }

    ctx.catalog->add_or_replace(outputs.slice(), normalized_source.view());

    if (!register_import_outputs(*ctx.registry, outputs.slice())) {
        FR_LOG_ERR("[Cooker] Failed to register one or more imported cooked assets.");
        return result;
    }

    if (!ctx.manifest_path.is_empty()) {
        if (!ctx.catalog->build_loose_manifest(ctx.manifest_path)) {
            FR_LOG_ERR("[Cooker] Failed to rebuild development asset manifest: {}",
                       ctx.manifest_path);
            return result;
        }
    }

    result.ok = true;
    result.mesh_path = String::from_view(ctx.alloc, cooked_mesh_path.view());
    result.mesh_id = AssetId::from_logical_path(result.mesh_path.view());
    result.output_count = outputs.size();

    FR_LOG("[Cooker] Imported glTF model '{}' as '{}'. Outputs: {}.", normalized_source.view(),
           result.mesh_path.view(), result.output_count);

    return result;
}

} // namespace fr::asscooker
