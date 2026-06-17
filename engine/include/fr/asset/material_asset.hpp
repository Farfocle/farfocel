/**
 * @file material_asset.hpp
 * @author Tfoedy
 * @brief Runtime decoding helpers for cooked material assets.
 */

#pragma once

#include <type_traits>

#include "fr/core/math.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_id.hpp"
#include "fr/asset/material_format.hpp"

namespace fr {

/// @brief Decoded material asset data.
struct MaterialAssetData {
    AssetId albedo_texture{};
    AssetId normal_texture{};
    AssetId extra_texture{};

    Vec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};

    F32 metallic_factor{0.0f};
    F32 roughness_factor{1.0f};
    F32 alpha{1.0f};
    F32 alpha_cutoff{0.5f};

    MaterialShadingModel shading_model{MaterialShadingModel::PBR};
    MaterialBlendMode blend_mode{MaterialBlendMode::Opaque};
};

namespace impl {

template <typename T>
[[nodiscard]] inline bool read_cooked_material_object(Slice<const Byte> bytes, USize offset,
                                                      T &out) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
        return false;
    }

    fr::mem::copy_raw_range(bytes.data() + offset, sizeof(T), reinterpret_cast<Byte *>(&out));
    return true;
}

[[nodiscard]] inline bool
verify_cooked_material_header(const CookedMaterialHeader &header) noexcept {
    return header.verify[0] == 'F' && header.verify[1] == 'M' && header.verify[2] == 'A' &&
           header.verify[3] == 'T' && header.version == 1;
}

} // namespace impl

/// @brief Decodes .fmat bytes into material asset data.
[[nodiscard]] inline bool load_cooked_material(Slice<const Byte> bytes,
                                               MaterialAssetData &out_material) noexcept {
    CookedMaterialHeader header{};

    if (!impl::read_cooked_material_object(bytes, 0, header)) {
        return false;
    }

    if (!impl::verify_cooked_material_header(header)) {
        return false;
    }

    out_material.albedo_texture = header.albedo_texture;
    out_material.normal_texture = header.normal_texture;
    out_material.extra_texture = header.extra_texture;

    out_material.base_color_factor = Vec4(header.base_color_factor[0], header.base_color_factor[1],
                                          header.base_color_factor[2], header.base_color_factor[3]);

    out_material.metallic_factor = header.metallic_factor;
    out_material.roughness_factor = header.roughness_factor;
    out_material.alpha = header.alpha;
    out_material.alpha_cutoff = header.alpha_cutoff;

    out_material.shading_model = header.shading_model;
    out_material.blend_mode = header.blend_mode;

    return true;
}

} // namespace fr
