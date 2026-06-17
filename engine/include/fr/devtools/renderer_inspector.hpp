/**
 * @file renderer_inspector.hpp
 * @brief Renderer debug & settings inspector panel.
 */

#pragma once

#include <imgui.h>

#include "fr/asset/material_format.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/devtools/gizmo.hpp"
#include "fr/devtools/render_settings_panel.hpp"
#include "fr/devtools/state.hpp"
#include "fr/devtools/stats_panel.hpp"

namespace fr::devtools {

// ============================================================ Shading Override

enum class ShadingOverride : S32 {
    MaterialDefault = 0,
    Unlit = 1,
    Standard = 2,
    PBR = 3,
};

inline const char *shading_override_name(ShadingOverride mode) noexcept {
    switch (mode) {
    case ShadingOverride::MaterialDefault:
        return "Material Default";
    case ShadingOverride::Unlit:
        return "Force Unlit";
    case ShadingOverride::Standard:
        return "Force Standard";
    case ShadingOverride::PBR:
        return "Force PBR";
    default:
        return "Unknown";
    }
}

inline void draw_shading_override_combo(DevToolsState &tools) noexcept {
    const char *items[] = {"Material Default", "Force Unlit", "Force Standard", "Force PBR"};
    if (ImGui::Combo("Shading Override##combo", &tools.shading_override, items, 4)) {
        if (tools.shading_override < 0)
            tools.shading_override = 0;
        if (tools.shading_override > 3)
            tools.shading_override = 3;
    }
    ImGui::TextDisabled(
        "%s", shading_override_name(static_cast<ShadingOverride>(tools.shading_override)));
}

/// @brief Applies shading override to all materials in a submission.
inline void apply_shading_override(RenderFrameSubmission &submission,
                                   ShadingOverride override_mode) noexcept {
    if (override_mode == ShadingOverride::MaterialDefault) {
        return;
    }

    U32 shading_model = static_cast<U32>(MaterialShadingModel::PBR);

    if (override_mode == ShadingOverride::Unlit) {
        shading_model = static_cast<U32>(MaterialShadingModel::Unlit);
    } else if (override_mode == ShadingOverride::Standard) {
        shading_model = static_cast<U32>(MaterialShadingModel::Standard);
    }

    for (USize i = 0; i < submission.materials.size(); ++i) {
        submission.materials[i].shading_model = shading_model;
    }
}

// ====================================================== Renderer inspector panel

/// @brief Draws the renderer / stats inspector as a tabbed panel.
/// Must be called between ImGui::Begin / ImGui::End.
inline void renderer_inspector_panel(World &world, DevToolsState &tools) noexcept {
    if (!ImGui::BeginTabBar("##renderer_inspector_tabs")) {
        return;
    }

    if (ImGui::BeginTabItem("Renderer##renderer_tab")) {
        ImGui::PushID("render_settings");
        draw_renderer_settings_panel(world.get_resource<fr::SceneRenderSettings>());
        ImGui::PopID();

        ImGui::SeparatorText("Material Debug##material_debug_sep");
        draw_shading_override_combo(tools);

        ImGui::SeparatorText("Transform Gizmo##gizmo_sep");
        draw_transform_gizmo_toolbar(tools);

        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Stats##stats_tab")) {
        ImGui::PushID("stats_panel");
        stats_panel(world, tools);
        ImGui::PopID();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

} // namespace fr::devtools
