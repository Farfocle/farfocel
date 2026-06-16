/**
 * @file render_settings_panel.hpp
 * @author Tfoedy
 * @brief ImGui renderer settings panel for runtime devtools.
 */

#pragma once

#include <imgui.h>

#include "fr/core/typedefs.hpp"
#include "fr/devtools/devtools_state.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/scene/render_scene_extractor.hpp"

namespace fr::devtools {

inline const char *render_debug_mode_name(RenderDebugMode mode) noexcept {
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

inline void draw_debug_mode_combo(RenderDebugSettings &debug) noexcept {
    constexpr RenderDebugMode modes[] = {
        RenderDebugMode::Final,        RenderDebugMode::Albedo,
        RenderDebugMode::Normal,       RenderDebugMode::MetallicSpecular,
        RenderDebugMode::Roughness,    RenderDebugMode::AmbientOcclusion,
        RenderDebugMode::ShadingModel, RenderDebugMode::Shadow,
        RenderDebugMode::Hbao,
    };

    const char *preview = render_debug_mode_name(debug.mode);

    if (ImGui::BeginCombo("Debug View", preview)) {
        for (RenderDebugMode mode : modes) {
            const bool selected = debug.mode == mode;

            if (ImGui::Selectable(render_debug_mode_name(mode), selected)) {
                debug.mode = mode;
            }

            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::DragInt("Debug Flags", reinterpret_cast<S32 *>(&debug.flags), 1.0f, 0, 0x7FFFFFFF);
}

inline void draw_lighting_settings(RenderLightingSettings &lighting) noexcept {
    ImGui::DragFloat("Exposure", &lighting.exposure, 0.01f, 0.01f, 64.0f);
    ImGui::DragFloat("PBR Ambient", &lighting.pbr_ambient_strength, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Standard Ambient", &lighting.standard_ambient_strength, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Standard Specular", &lighting.standard_specular_default, 0.001f, 0.0f, 10.0f);
}

inline void draw_ao_settings(RenderAmbientOcclusionSettings &ao) noexcept {
    ImGui::Checkbox("AO Enabled", &ao.enabled);
    ImGui::DragFloat("AO Radius", &ao.radius, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("AO Intensity", &ao.intensity, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("AO Bias", &ao.bias, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("AO Power", &ao.power, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("AO Thickness", &ao.thickness, 0.01f, 0.0f, 100.0f);
}

inline void draw_ibl_settings(RenderIblSettings &ibl) noexcept {
    ImGui::Checkbox("IBL Enabled", &ibl.enabled);
    ImGui::DragFloat("Diffuse Strength", &ibl.diffuse_strength, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Specular Strength", &ibl.specular_strength, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Occlusion Strength", &ibl.occlusion_strength, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Occlusion Power", &ibl.occlusion_power, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Sky Visibility", &ibl.sky_visibility_strength, 0.01f, 0.0f, 10.0f);
}

inline void draw_directional_shadow_settings(RenderDirectionalShadowSettings &settings) noexcept {
    ImGui::DragFloat3("Cascade Splits", &settings.cascade_splits.x, 0.25f, 0.1f, 10000.0f);
    ImGui::DragFloat3("Cascade Half Extents", &settings.cascade_half_extents.x, 0.25f, 0.1f,
                      10000.0f);
    ImGui::DragFloat3("Cascade Depth Ranges", &settings.cascade_depth_ranges.x, 0.25f, 0.1f,
                      10000.0f);

    ImGui::Separator();

    ImGui::DragFloat("Min Bias", &settings.min_bias, 0.0001f, 0.0f, 1.0f, "%.6f");
    ImGui::DragFloat("Slope Bias", &settings.slope_bias, 0.0001f, 0.0f, 1.0f, "%.6f");
    ImGui::DragFloat("Cascade Bias Scale", &settings.cascade_bias_scale, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Shadow Strength", &settings.shadow_strength, 0.01f, 0.0f, 1.0f);

    ImGui::Separator();

    ImGui::DragFloat("Filter Radius Texels", &settings.filter_radius_texels, 0.01f, 0.0f, 16.0f);
    ImGui::DragFloat("Cascade Filter Scale", &settings.cascade_filter_scale, 0.01f, 0.0f, 10.0f);
}

/**
 * @brief Draws renderer settings stored in DevToolsState.
 */
inline void render_settings_panel(DevToolsState &tools) noexcept {
    if (ImGui::CollapsingHeader("Debug View", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_debug_mode_combo(tools.debug);
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_lighting_settings(tools.lighting);
    }

    if (ImGui::CollapsingHeader("Directional Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_directional_shadow_settings(tools.directional_shadow_settings);
    }

    if (ImGui::CollapsingHeader("Ambient Occlusion")) {
        draw_ao_settings(tools.ao);
    }

    if (ImGui::CollapsingHeader("IBL")) {
        draw_ibl_settings(tools.ibl);
    }
}

} // namespace fr::devtools
