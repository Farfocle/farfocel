/**
 * @file primitive_panel.hpp
 * @brief Devtools panel for spawning primitive meshes.
 */

#pragma once

#include "fr/data/world.hpp"
#include "fr/devtools/state.hpp"

namespace fr::devtools {

/// @brief Draws Cube / Plane / Grid spawn buttons.
/// Uses tools.spawn_transform for the initial position.
void draw_primitive_spawn_panel(World &world, DevToolsState &tools) noexcept;

} // namespace fr::devtools
