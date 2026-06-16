/**
 * @file spawn_panel.hpp
 * @brief Devtools spawn and scene IO panel.
 */

#pragma once

#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/state.hpp"

namespace fr::devtools {

/// @brief Draws the full spawn + scene IO panel.
/// Spawns are placed at the camera's "in front" position automatically.
void draw_spawn_panel(World &world, DevToolsState &tools) noexcept;

} // namespace fr::devtools
