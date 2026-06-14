/**
 * @file material_compiler.cpp
 * @author Tfoedy
 * @brief Material asset compiler.
 */

#include "material_compiler.hpp"

#include "fr/asset/material_format.hpp"
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

[[nodiscard]] bool is_valid_shading_model(MaterialShadingModel model) noexcept {
    return model == MaterialShadingModel::Unlit || model == MaterialShadingModel::Standard ||
           model == MaterialShadingModel::PBR;
}

[[nodiscard]] bool is_valid_blend_mode(MaterialBlendMode mode) noexcept {
    return mode == MaterialBlendMode::Opaque || mode == MaterialBlendMode::Masked ||
           mode == MaterialBlendMode::Transparent;
}

[[nodiscard]] bool validate_material(const RawMaterial &material, StringView output_path) noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot compile material with empty output path.");
        return false;
    }

    if (material.metallic_factor < 0.0f || material.metallic_factor > 1.0f) {
        FR_LOG_ERR("[Cooker] Material metallic factor is out of range: {}", output_path);
        return false;
    }

    if (material.roughness_factor < 0.0f || material.roughness_factor > 1.0f) {
        FR_LOG_ERR("[Cooker] Material roughness factor is out of range: {}", output_path);
        return false;
    }

    if (material.alpha < 0.0f || material.alpha > 1.0f) {
        FR_LOG_ERR("[Cooker] Material alpha is out of range: {}", output_path);
        return false;
    }

    if (material.alpha_cutoff < 0.0f || material.alpha_cutoff > 1.0f) {
        FR_LOG_ERR("[Cooker] Material alpha cutoff is out of range: {}", output_path);
        return false;
    }

    for (USize i = 0; i < 4; ++i) {
        if (material.base_color_factor[i] < 0.0f) {
            FR_LOG_ERR("[Cooker] Material base color factor {} is negative: {}", i, output_path);
            return false;
        }
    }

    if (!is_valid_shading_model(material.shading_model)) {
        FR_LOG_ERR("[Cooker] Material has invalid shading model: {}", output_path);
        return false;
    }

    if (!is_valid_blend_mode(material.blend_mode)) {
        FR_LOG_ERR("[Cooker] Material has invalid blend mode: {}", output_path);
        return false;
    }

    return true;
}

} // namespace

bool compile_material(const RawMaterial &material, StringView output_path) noexcept {
    if (!validate_material(material, output_path)) {
        return false;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    CookedMaterialHeader header{};
    header.verify[0] = 'F';
    header.verify[1] = 'M';
    header.verify[2] = 'A';
    header.verify[3] = 'T';
    header.version = 1;

    header.albedo_texture = material.albedo_texture;
    header.normal_texture = material.normal_texture;
    header.extra_texture = material.extra_texture;

    copy_array(material.base_color_factor, header.base_color_factor);

    header.metallic_factor = material.metallic_factor;
    header.roughness_factor = material.roughness_factor;
    header.alpha = material.alpha;
    header.alpha_cutoff = material.alpha_cutoff;

    header.shading_model = material.shading_model;
    header.blend_mode = material.blend_mode;

    String out_path = String::from_view(alloc, output_path);

    if (!file::write_all_bytes(out_path, Slice<const Byte>(reinterpret_cast<const Byte *>(&header),
                                                           sizeof(CookedMaterialHeader)))) {
        FR_LOG_ERR("[Cooker] Failed to write cooked material: {}", output_path);
        return false;
    }

    return true;
}

} // namespace fr::asscooker
