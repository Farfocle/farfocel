/**
 * @file stats_panel.cpp
 * @brief Basic runtime statistics panel implementations.
 */

#include "fr/devtools/stats_panel.hpp"

#include <imgui.h>

#include "fr/scene/render_parts.hpp"

namespace fr::devtools {

SceneObjectStats collect_scene_object_stats(World &world) noexcept {
    SceneObjectStats stats{};

    world.each_alive_thing([&](Thing thing) noexcept {
        if (thing.is_nil()) {
            return;
        }

        ++stats.things;

        if (world.try_get<MeshRendererPart>(thing))     ++stats.meshes;
        if (world.try_get<CameraPart>(thing))           ++stats.cameras;
        if (world.try_get<DirectionalLightPart>(thing)) ++stats.directional_lights;
        if (world.try_get<PointLightPart>(thing))       ++stats.point_lights;
        if (world.try_get<SpotLightPart>(thing))        ++stats.spot_lights;
    });

    return stats;
}

void draw_submit_stats(const char *label, const RenderSubmitStats &stats) noexcept {
    if (ImGui::TreeNode(label)) {
        ImGui::Text("Total:   %u", stats.total_submeshes);
        ImGui::Text("Visible: %u", stats.visible_submeshes);
        ImGui::Text("Culled:  %u", stats.culled_submeshes);
        ImGui::Text("Skipped: %u", stats.skipped_submeshes);
        ImGui::TreePop();
    }
}

void stats_panel(World &world, const DevToolsState &tools) noexcept {
    ImGui::SeparatorText("Frame");
    ImGui::Text("FPS: %.1f", static_cast<F64>(tools.fps));
    ImGui::Text("dt:  %.2f ms", static_cast<F64>(tools.dt * 1000.0f));
    ImGui::Text("Viewport: %u x %u", tools.viewport_width, tools.viewport_height);
    ImGui::Text("Camera: %s", tools.has_main_camera ? "found" : "none");

    ImGui::SeparatorText("Render");
    draw_submit_stats("Geometry##geom_stats", tools.geometry_stats);
    draw_submit_stats("Shadows##shadow_stats", tools.shadow_stats);

    ImGui::SeparatorText("Scene");
    const SceneObjectStats scene = collect_scene_object_stats(world);
    ImGui::Text("Things:       %zu", scene.things);
    ImGui::Text("Meshes:       %zu", scene.meshes);
    ImGui::Text("Cameras:      %zu", scene.cameras);
    ImGui::Text("Dir Lights:   %zu", scene.directional_lights);
    ImGui::Text("Point Lights: %zu", scene.point_lights);
    ImGui::Text("Spot Lights:  %zu", scene.spot_lights);
}

} // namespace fr::devtools
