/**
 * @file dev_asset_importer.hpp
 * @author Tfoedy
 * @brief Development source asset import helpers.
 */

#pragma once

#include "fr/asscooker/dev_asset_catalog.hpp"
#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr::asscooker {

/**
 * @brief Context required to import source assets into cooked development assets.
 *
 * @details
 * cooked_root is the base directory for generated cooked assets. For example, when cooked_root is
 * "assets", importing "chair.gltf" produces:
 *
 * assets/models/imported/chair/chair.fmesh
 */
struct DevAssetImportContext {
    Alloc *alloc{nullptr};

    AssetRegistry *registry{nullptr};
    DevAssetCatalog *catalog{nullptr};

    StringView cooked_root{"assets"};
    StringView manifest_path{"assets/dev.fmanifest"};

    [[nodiscard]] bool is_valid() const noexcept {
        return alloc != nullptr && registry != nullptr && catalog != nullptr;
    }
};

/**
 * @brief Result of importing one glTF model.
 */
struct ImportedModelResult {
    bool ok{false};

    String mesh_path{};
    AssetId mesh_id{};

    USize output_count{0};
};

/**
 * @brief Imports a glTF source model into cooked development assets.
 *
 * @details
 * The returned mesh_path is ready to be assigned to MeshRendererPart::mesh_path.
 */
[[nodiscard]] ImportedModelResult import_gltf_model(DevAssetImportContext &ctx,
                                                    StringView source_path,
                                                    StringView import_name = {},
                                                    bool force = true) noexcept;

} // namespace fr::asscooker
