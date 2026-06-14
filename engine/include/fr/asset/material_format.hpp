/**
 * @file material_format.hpp
 * @author Tfoedy
 * @brief Cooked material asset format.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Material shading model stored in .fmat.
 */
enum class MaterialShadingModel : U32 {
    Unlit = 0,
    Standard = 1,
    PBR = 2,
};

/**
 * @brief Material blend mode stored in .fmat.
 */
enum class MaterialBlendMode : U32 {
    Opaque = 0,
    Masked = 1,
    Transparent = 2,
};

#pragma pack(push, 1)

/**
 * @brief Header and payload of a cooked .fmat file.
 *
 * @details
 * Texture fields store logical AssetIds. Invalid ids mean that the material does not reference the
 * corresponding texture.
 */
struct CookedMaterialHeader {
    char verify[4]{'F', 'M', 'A', 'T'};
    U32 version{1};

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

    U32 reserved0{0};
    U32 reserved1{0};
};

#pragma pack(pop)

static_assert(sizeof(AssetId) == sizeof(U64), "AssetId must remain a 64-bit disk value");

} // namespace fr
