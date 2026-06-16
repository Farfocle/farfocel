/**
 * @file transform_gizmo.hpp
 * @brief ImGuizmo transform manipulation helpers for runtime devtools.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/state.hpp"

namespace fr::devtools {

/// @brief Draws the ImGuizmo gizmo overlay for the selected thing.
void draw_transform_gizmo(World &world, DevToolsState &tools, F32 viewport_width,
                          F32 viewport_height) noexcept;

/// @brief Draws gizmo toolbar controls (operation, space, snap) inline.
void draw_transform_gizmo_toolbar(DevToolsState &tools) noexcept;

/// @brief Handles keyboard shortcuts: T=translate, R=rotate, Y=scale.
void update_transform_gizmo_shortcuts(DevToolsState &tools) noexcept;

} // namespace fr::devtools
