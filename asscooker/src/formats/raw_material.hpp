/**
 * @file raw_material.hpp
 * @author Tfoedy
 * @brief Intermediate material data used by the asset cooker.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/asset/material_format.hpp"
#include "fr/core/typedefs.hpp"

namespace fr::asscooker {

/**
 * @brief Material data ready for .fmat compilation.
 */
struct RawMaterial {
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

} // namespace fr::asscooker
