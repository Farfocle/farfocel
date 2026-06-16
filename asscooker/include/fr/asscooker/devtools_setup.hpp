/**
 * @file devtools_setup.hpp
 * @author Kiju
 *
 * @brief Development application setup helpers: shader cooking, manifest loading.
 */

#pragma once

#include "fr/asset/asset_manifest.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/asset/asset_storage.hpp"
#include "fr/asscooker/asscooker.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {

/**
 * @brief Input descriptor for cooking one shader file pair.
 */
struct ShaderCookInput {
    fr::AssetId id{};
    fr::StringView vertex_path{};
    fr::StringView fragment_path{};
    fr::StringView output_path{};
};

/**
 * @brief Registers cooked asset outputs with the asset registry.
 */
inline bool register_cooked_outputs(
    fr::AssetRegistry &registry,
    fr::Slice<const CookedAssetOutput> outputs) noexcept {

    bool ok = true;

    for (const CookedAssetOutput &output : outputs) {
        if (!output.id.is_valid() || output.kind == fr::AssetKind::Unknown ||
            output.path.size() == 0) {
            FR_LOG_ERR("[DevSetup] Invalid cooked output record.");
            ok = false;
            continue;
        }

        ok = registry.register_loose_asset(output.id, output.kind, output.path.view(),
                                           output.content_hash) &&
             ok;
    }

    return ok;
}

/**
 * @brief Cooks the provided shader pairs and registers them in the asset registry.
 *
 * @param alloc     Allocator for intermediate storage.
 * @param registry  Asset registry to register outputs into.
 * @param shaders   Slice of shader cook inputs.
 * @return true if all shaders cooked and registered successfully.
 */
inline bool cook_and_register_shaders(fr::Alloc *alloc, fr::AssetRegistry &registry,
                                      fr::Slice<const ShaderCookInput> shaders) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    fr::DynamicArray<CookedAssetOutput> outputs(alloc);

    for (const ShaderCookInput &shader : shaders) {
        CookOptions options{};
        options.output_id = shader.id;
        options.force = true;

        if (!cook_shader_ex(shader.vertex_path, shader.fragment_path, shader.output_path,
                            &outputs, options)) {
            FR_LOG_ERR("[DevSetup] Failed to cook shader: {}", shader.output_path);
            return false;
        }
    }

    return register_cooked_outputs(registry, outputs.slice());
}

/**
 * @brief Loads a development asset manifest from disk if the file exists.
 *
 * @details
 * If the manifest file does not exist this is treated as a non-error (first-run case).
 *
 * @return true on success or if the file was not found.
 */
inline bool load_dev_manifest_if_exists(fr::Alloc *alloc, fr::AssetRegistry &registry,
                                        fr::AssetStorage &storage,
                                        fr::StringView manifest_path) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (manifest_path.is_empty()) {
        return true;
    }

    fr::String path = fr::String::from_view(alloc, manifest_path);

    if (!fr::file::exists(path)) {
        FR_LOG("[DevSetup] Development asset manifest not found: {}", manifest_path);
        return true;
    }

    if (!fr::load_asset_manifest(alloc, manifest_path, registry, storage)) {
        FR_LOG_ERR("[DevSetup] Failed to load development asset manifest: {}", manifest_path);
        return false;
    }

    FR_LOG_OK("[DevSetup] Loaded development asset manifest: {}", manifest_path);
    return true;
}

} // namespace fr::asscooker
