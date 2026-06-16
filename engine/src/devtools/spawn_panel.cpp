/**
 * @file spawn_panel.cpp
 * @brief Devtools spawn and scene IO panel implementation.
 */

#include "fr/devtools/spawn_panel.hpp"

#include <imgui.h>

#include "fr/devtools/world_actions.hpp"

namespace fr::devtools {

void draw_spawn_panel(World &world, DevToolsState &tools) noexcept {
    // ------------------------------------------------------------------ Spawn
    ImGui::SeparatorText("Primitives");

    if (ImGui::Button("Cube")) {
        spawn_cube(world, tools, spawn_in_front_of_camera(world));
    }
    ImGui::SameLine();
    if (ImGui::Button("Plane")) {
        spawn_plane(world, tools, spawn_in_front_of_camera(world));
    }
    ImGui::SameLine();
    if (ImGui::Button("Grid")) {
        spawn_grid(world, tools, spawn_in_front_of_camera(world));
    }

    ImGui::SeparatorText("Mesh");

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##mesh_path", tools.mesh_path, sizeof(tools.mesh_path));
    if (ImGui::Button("Spawn Mesh")) {
        spawn_mesh(world, tools, StringView(tools.mesh_path),
                   spawn_in_front_of_camera(world));
    }

    ImGui::SeparatorText("Lights");

    if (ImGui::Button("Directional")) {
        spawn_directional_light(world, tools, spawn_in_front_of_camera(world));
    }
    ImGui::SameLine();
    if (ImGui::Button("Point")) {
        spawn_point_light(world, tools, spawn_in_front_of_camera(world));
    }
    ImGui::SameLine();
    if (ImGui::Button("Spot")) {
        spawn_spot_light(world, tools, spawn_in_front_of_camera(world));
    }

    ImGui::SeparatorText("Other");

    if (ImGui::Button("Empty")) {
        spawn_base(world, tools, spawn_in_front_of_camera(world));
    }
    ImGui::SameLine();
    if (ImGui::Button("Camera")) {
        spawn_camera(world, tools, spawn_in_front_of_camera(world));
    }

    // --------------------------------------------------------------- Scene IO
    ImGui::SeparatorText("Scene IO");

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##scene_path", tools.scene_path, sizeof(tools.scene_path));

    if (ImGui::Button("Save")) {
        tools.request_save_scene = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        tools.request_load_scene = true;
    }
}

} // namespace fr::devtools
