/**
 * @file scene_render_settings.hpp
 * @author Tfoedy
 * @brief Scene-owned renderer settings.
 */

#pragma once

#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/scene/render_scene_extractor.hpp"

namespace fr {

/**
 * @brief Persistent per-scene renderer settings.
 */
struct SceneRenderSettingsPart {
    RenderLightingSettings lighting{};
    RenderAmbientOcclusionSettings ao{};
    RenderIblSettings ibl{};
    RenderDebugSettings debug{};
    RenderDirectionalShadowSettings directional_shadow_settings{};

    SceneRenderSettingsPart() noexcept = default;

    template <typename Archive>
    void shape(Archive &archive) noexcept {
        archive.prop("exposure", lighting.exposure);
        archive.prop("pbr_ambient_strength", lighting.pbr_ambient_strength);
        archive.prop("standard_ambient_strength", lighting.standard_ambient_strength);
        archive.prop("standard_specular_default", lighting.standard_specular_default);

        archive.prop("ao_enabled", ao.enabled);
        archive.prop("ao_radius", ao.radius);
        archive.prop("ao_intensity", ao.intensity);
        archive.prop("ao_bias", ao.bias);
        archive.prop("ao_power", ao.power);
        archive.prop("ao_thickness", ao.thickness);

        archive.prop("ibl_enabled", ibl.enabled);
        archive.prop("ibl_diffuse_strength", ibl.diffuse_strength);
        archive.prop("ibl_specular_strength", ibl.specular_strength);
        archive.prop("ibl_occlusion_strength", ibl.occlusion_strength);
        archive.prop("ibl_occlusion_power", ibl.occlusion_power);
        archive.prop("ibl_sky_visibility_strength", ibl.sky_visibility_strength);

        U32 debug_mode = static_cast<U32>(debug.mode);
        archive.prop("debug_mode", debug_mode);

        if constexpr (Archive::action == ArchiveAction::Read) {
            if (debug_mode > static_cast<U32>(RenderDebugMode::Hbao)) {
                debug_mode = static_cast<U32>(RenderDebugMode::Final);
            }

            debug.mode = static_cast<RenderDebugMode>(debug_mode);
        }

        archive.prop("debug_flags", debug.flags);

        archive.prop("directional_cascade_splits", directional_shadow_settings.cascade_splits);
        archive.prop("directional_cascade_half_extents",
                     directional_shadow_settings.cascade_half_extents);
        archive.prop("directional_cascade_depth_ranges",
                     directional_shadow_settings.cascade_depth_ranges);

        archive.prop("directional_min_bias", directional_shadow_settings.min_bias);
        archive.prop("directional_slope_bias", directional_shadow_settings.slope_bias);
        archive.prop("directional_cascade_bias_scale",
                     directional_shadow_settings.cascade_bias_scale);
        archive.prop("directional_shadow_strength", directional_shadow_settings.shadow_strength);
        archive.prop("directional_filter_radius_texels",
                     directional_shadow_settings.filter_radius_texels);
        archive.prop("directional_cascade_filter_scale",
                     directional_shadow_settings.cascade_filter_scale);
    }
};

} // namespace fr

FR_TYPE(fr::SceneRenderSettingsPart);
