/**
 * @file render_settings_panel.cpp
 * @brief ImGui renderer settings panel implementations.
 */

#include "fr/devtools/render_settings_panel.hpp"

#include <imgui.h>

namespace fr::devtools {

const char *render_debug_mode_name(RenderDebugMode mode) noexcept {
    switch (mode) {
    case RenderDebugMode::Final:
        return "Final";
    case RenderDebugMode::Albedo:
        return "Albedo";
    case RenderDebugMode::Normal:
        return "Normal";
    case RenderDebugMode::MetallicSpecular:
        return "Metallic / Specular";
    case RenderDebugMode::Roughness:
        return "Roughness";
    case RenderDebugMode::AmbientOcclusion:
        return "Ambient Occlusion";
    case RenderDebugMode::ShadingModel:
        return "Shading Model";
    case RenderDebugMode::Shadow:
        return "Shadow";
    case RenderDebugMode::Hbao:
        return "HBAO";
    default:
        return "Unknown";
    }
}

void draw_debug_mode_combo(RenderDebugSettings &debug) noexcept {
    const char *items[] = {
        "Final",
        "Albedo",
        "Normal",
        "Metallic / Specular",
        "Roughness",
        "Ambient Occlusion",
        "Shading Model",
        "Shadow",
        "HBAO",
    };

    S32 mode = static_cast<S32>(debug.mode);
    if (ImGui::Combo("Debug Mode##debug_mode", &mode, items, 9)) {
        if (mode >= 0 && mode < 9) {
            debug.mode = static_cast<RenderDebugMode>(mode);
        }
    }
}

void draw_lighting_settings(RenderLightingSettings &lighting) noexcept {
    ImGui::DragFloat("Exposure##exposure", &lighting.exposure, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("PBR Ambient##pbr_ambient", &lighting.pbr_ambient_strength, 0.001f, 0.0f,
                     1.0f);
    ImGui::DragFloat("Standard Ambient##std_ambient", &lighting.standard_ambient_strength, 0.001f,
                     0.0f, 1.0f);
    ImGui::DragFloat("Standard Specular##std_specular", &lighting.standard_specular_default,
                     0.001f, 0.0f, 1.0f);
}

void draw_ao_settings(RenderAmbientOcclusionSettings &ao) noexcept {
    ImGui::Checkbox("AO Enabled##ao_enabled", &ao.enabled);

    if (!ao.enabled) {
        return;
    }

    ImGui::DragFloat("AO Radius##ao_radius", &ao.radius, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("AO Intensity##ao_intensity", &ao.intensity, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("AO Bias##ao_bias", &ao.bias, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("AO Power##ao_power", &ao.power, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("AO Thickness##ao_thickness", &ao.thickness, 0.01f, 0.0f, 10.0f);
}

void draw_ibl_settings(RenderIblSettings &ibl) noexcept {
    ImGui::Checkbox("IBL Enabled##ibl_enabled", &ibl.enabled);

    if (!ibl.enabled) {
        return;
    }

    ImGui::DragFloat("Diffuse Strength##ibl_diff", &ibl.diffuse_strength, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Specular Strength##ibl_spec", &ibl.specular_strength, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Occlusion Strength##ibl_occ", &ibl.occlusion_strength, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Occlusion Power##ibl_occ_pow", &ibl.occlusion_power, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Sky Visibility##ibl_sky", &ibl.sky_visibility_strength, 0.01f, 0.0f, 2.0f);
}

void draw_directional_shadow_settings(RenderDirectionalShadowSettings &settings) noexcept {
    ImGui::DragFloat3("Cascade Splits##csm_splits", &settings.cascade_splits.x, 0.5f, 0.0f,
                      1000.0f);
    ImGui::DragFloat3("Cascade Half Extents##csm_ext", &settings.cascade_half_extents.x, 0.5f,
                      0.0f, 1000.0f);
    ImGui::DragFloat3("Cascade Depth Ranges##csm_depth", &settings.cascade_depth_ranges.x, 0.5f,
                      0.0f, 1000.0f);
    ImGui::DragFloat("Min Bias##csm_min_bias", &settings.min_bias, 0.0001f, 0.0f, 0.1f, "%.4f");
    ImGui::DragFloat("Slope Bias##csm_slope_bias", &settings.slope_bias, 0.0001f, 0.0f, 0.1f,
                     "%.4f");
    ImGui::DragFloat("Cascade Bias Scale##csm_bias_scale", &settings.cascade_bias_scale, 0.0001f,
                     0.0f, 1.0f, "%.4f");
    ImGui::DragFloat("Shadow Strength##csm_strength", &settings.shadow_strength, 0.01f, 0.0f,
                     1.0f);
    ImGui::DragFloat("Filter Radius##csm_filter", &settings.filter_radius_texels, 0.05f, 0.0f,
                     8.0f);
    ImGui::DragFloat("Cascade Filter Scale##csm_filter_scale", &settings.cascade_filter_scale,
                     0.01f, 0.0f, 4.0f);
}

void draw_renderer_settings_panel(fr::SceneRenderSettings &settings) noexcept {
    if (ImGui::CollapsingHeader("Debug Output##debug_hdr")) {
        ImGui::PushID("debug");
        draw_debug_mode_combo(settings.debug);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Lighting##lighting_hdr", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("lighting");
        draw_lighting_settings(settings.lighting);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Ambient Occlusion##ao_hdr")) {
        ImGui::PushID("ao");
        draw_ao_settings(settings.ao);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("IBL##ibl_hdr")) {
        ImGui::PushID("ibl");
        draw_ibl_settings(settings.ibl);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Directional Shadows##shadow_hdr")) {
        ImGui::PushID("shadow");
        draw_directional_shadow_settings(settings.directional_shadow_settings);
        ImGui::PopID();
    }
}

} // namespace fr::devtools
