/**
 * @file stats_panel.hpp
 * @author Tfoedy
 * @brief Basic runtime statistics panel for devtools.
 */

#pragma once

#include <imgui.h>

#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"
#include "fr/scene/render_extractor.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr::devtools {

struct DevToolsFrameStats {
    F32 dt{0.0f};
    F32 fps{0.0f};
    U32 viewport_width{0};
    U32 viewport_height{0};

    RenderSubmitStats geometry_stats{};
    RenderSubmitStats shadow_stats{};

    bool has_main_camera{false};
};

struct SceneObjectStats {
    USize things{0};
    USize meshes{0};
    USize cameras{0};
    USize directional_lights{0};
    USize point_lights{0};
    USize spot_lights{0};
};

inline SceneObjectStats collect_scene_object_stats(World &world) noexcept {
    SceneObjectStats stats{};
    stats.things = world.alive_thing_count();

    world.for_each_alive_thing([&](Thing thing) noexcept {
        if (thing.is_nil()) {
            return;
        }

        if (world.try_get<MeshRendererPart>(thing)) {
            ++stats.meshes;
        }

        if (world.try_get<CameraPart>(thing)) {
            ++stats.cameras;
        }

        if (world.try_get<DirectionalLightPart>(thing)) {
            ++stats.directional_lights;
        }

        if (world.try_get<PointLightPart>(thing)) {
            ++stats.point_lights;
        }

        if (world.try_get<SpotLightPart>(thing)) {
            ++stats.spot_lights;
        }
    });

    return stats;
}

inline void draw_submit_stats(const char *label, const RenderSubmitStats &stats) noexcept {
    if (!ImGui::TreeNode(label)) {
        return;
    }

    ImGui::Text("Total submeshes:   %u", stats.total_submeshes);
    ImGui::Text("Visible submeshes: %u", stats.visible_submeshes);
    ImGui::Text("Culled submeshes:  %u", stats.culled_submeshes);
    ImGui::Text("Skipped submeshes: %u", stats.skipped_submeshes);

    ImGui::TreePop();
}

inline void stats_panel(World &world, const DevToolsFrameStats &frame_stats) noexcept {
    ImGui::Text("FPS: %.1f", frame_stats.fps);
    ImGui::Text("Frame: %.3f ms", frame_stats.dt * 1000.0f);
    ImGui::Text("Viewport: %ux%u", frame_stats.viewport_width, frame_stats.viewport_height);
    ImGui::Text("Main Camera: %s", frame_stats.has_main_camera ? "yes" : "no");

    ImGui::Separator();

    const SceneObjectStats scene_stats = collect_scene_object_stats(world);

    ImGui::Text("Alive Things: %zu", scene_stats.things);
    ImGui::Text("Meshes: %zu", scene_stats.meshes);
    ImGui::Text("Cameras: %zu", scene_stats.cameras);
    ImGui::Text("Directional Lights: %zu", scene_stats.directional_lights);
    ImGui::Text("Point Lights: %zu", scene_stats.point_lights);
    ImGui::Text("Spot Lights: %zu", scene_stats.spot_lights);

    ImGui::Separator();

    draw_submit_stats("Geometry Submit", frame_stats.geometry_stats);
    draw_submit_stats("Shadow Submit", frame_stats.shadow_stats);
}

} // namespace fr::devtools
