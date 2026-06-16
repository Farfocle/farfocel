/**
 * @file primitive_panel.cpp
 * @brief Devtools primitive mesh spawn panel implementation.
 */

#include "fr/devtools/primitive_panel.hpp"

#include <imgui.h>

#include "fr/devtools/world_actions.hpp"

namespace fr::devtools {

void draw_primitive_spawn_panel(World &world, DevToolsState &tools) noexcept {
    if (ImGui::Button("Cube##spawn_prim_cube")) {
        spawn_cube(world, tools, spawn_in_front_of_camera(world));
    }
    ImGui::SameLine();
    if (ImGui::Button("Plane##spawn_prim_plane")) {
        spawn_plane(world, tools, spawn_in_front_of_camera(world));
    }
    ImGui::SameLine();
    if (ImGui::Button("Grid##spawn_prim_grid")) {
        spawn_grid(world, tools, spawn_in_front_of_camera(world));
    }
}

} // namespace fr::devtools
