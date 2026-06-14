/**
 * @file asscooker.hpp
 * @author Tfoedy
 * @brief Public API for the Farfocel Asset Cooker.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_kind.hpp"
#include "fr/asset/material_format.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr::asscooker {

/**
 * @brief Options shared by high-level cooking entry points.
 */
struct CookOptions {
    /**
     * @brief Explicit logical id for the primary cooked output.
     *
     * @details
     * When invalid, the cooker derives the id from the cooked output path. This is useful for
     * regular content. Built-in assets, such as renderer shaders, can pass stable named ids.
     */
    AssetId output_id{};

    /**
     * @brief Rebuild generated dependencies even if their cooked output already exists.
     */
    bool force{false};
};

/**
 * @brief One cooked runtime asset produced by the cooker.
 */
struct CookedAssetOutput {
    AssetId id{};
    AssetKind kind{AssetKind::Unknown};

    String path{};

    U64 content_hash{0};
};

/**
 * @brief Appends a cooked asset output record.
 */
void append_cooked_asset_output(DynamicArray<CookedAssetOutput> *outputs, AssetId id,
                                AssetKind kind, StringView path, U64 content_hash = 0) noexcept;

/**
 * @brief Creates an output AssetId from options or from a cooked logical path.
 */
[[nodiscard]] AssetId resolve_output_asset_id(StringView cooked_path,
                                              const CookOptions &options) noexcept;

/**
 * @brief Cooks a glTF mesh into .fmesh.
 *
 * @details
 * Referenced material and texture ids are derived from cooked logical paths.
 */
bool cook_mesh(StringView input_path, StringView output_path);

/**
 * @brief Cooks a glTF mesh and reports produced cooked assets.
 */
bool cook_mesh_ex(StringView input_path, StringView output_path,
                  DynamicArray<CookedAssetOutput> *outputs, CookOptions options = {}) noexcept;

/**
 * @brief Cooks an image into .ftex.
 */
bool cook_texture(StringView input_path, StringView output_path, bool is_srgb = false);

/**
 * @brief Cooks an image into .ftex and reports the produced texture asset.
 */
bool cook_texture_ex(StringView input_path, StringView output_path, bool is_srgb,
                     DynamicArray<CookedAssetOutput> *outputs, CookOptions options = {}) noexcept;

/**
 * @brief Cooks a vertex/fragment shader pair into .fshader.
 */
bool cook_shader(StringView vertex_path, StringView fragment_path, StringView output_path);

/**
 * @brief Cooks a vertex/fragment shader pair and reports the produced shader asset.
 */
bool cook_shader_ex(StringView vertex_path, StringView fragment_path, StringView output_path,
                    DynamicArray<CookedAssetOutput> *outputs, CookOptions options = {}) noexcept;

/**
 * @brief Material data used to build .fmat.
 */
struct MaterialCookDesc {
    AssetId albedo_texture{};
    AssetId normal_texture{};
    AssetId extra_texture{};

    F32 base_color_factor[4]{1.0f, 1.0f, 1.0f, 1.0f};

    F32 metallic_factor{0.0f};
    F32 roughness_factor{1.0f};
    F32 alpha{1.0f};
    F32 alpha_cutoff{0.5f};

    MaterialShadingModel shading_model{MaterialShadingModel::PBR};
    MaterialBlendMode blend_mode{MaterialBlendMode::Opaque};
};

/**
 * @brief Cooks material data into .fmat.
 */
bool cook_material(const MaterialCookDesc &desc, StringView output_path);

/**
 * @brief Cooks material data and reports the produced material asset.
 */
bool cook_material_ex(const MaterialCookDesc &desc, StringView output_path,
                      DynamicArray<CookedAssetOutput> *outputs, CookOptions options = {}) noexcept;

/**
 * @brief One cooked asset passed to .fpack generation.
 *
 * @details
 * id should match the logical AssetId used by cooked references and manifests.
 */
struct PackAssetInput {
    AssetId id{};
    AssetKind kind{AssetKind::Unknown};
    StringView path{};
    U64 content_hash{0};
};

/**
 * @brief Builds a cooked asset pack.
 */
bool build_pack(Slice<const PackAssetInput> assets, StringView output_path);

/**
 * @brief One pack path included in .fmanifest.
 */
struct ManifestPackInput {
    StringView path{};
};

/**
 * @brief One loose cooked asset entry included in .fmanifest.
 *
 * @details
 * id is logical. path is the cooked file path stored in the manifest.
 */
struct ManifestLooseInput {
    AssetId id{};
    AssetKind kind{AssetKind::Unknown};
    StringView path{};
    U64 content_hash{0};
};

/**
 * @brief Asset manifest build input.
 */
struct ManifestBuildDesc {
    Slice<const ManifestPackInput> packs{};
    Slice<const ManifestLooseInput> loose_assets{};
};

/**
 * @brief Builds a runtime .fmanifest.
 */
bool build_manifest(const ManifestBuildDesc &desc, StringView output_path);

} // namespace fr::asscooker
