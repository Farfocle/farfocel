/**
 * @file stats_panel.hpp
 * @brief Basic runtime statistics panel for devtools.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/state.hpp"
#include "fr/scene/render_scene_extractor.hpp"

namespace fr::devtools {

struct SceneObjectStats {
    USize things{0};
    USize meshes{0};
    USize cameras{0};
    USize directional_lights{0};
    USize point_lights{0};
    USize spot_lights{0};
};

SceneObjectStats collect_scene_object_stats(World &world) noexcept;
void draw_submit_stats(const char *label, const RenderSubmitStats &stats) noexcept;

/// @brief Draws FPS, viewport, render submit stats, and scene object counts.
void stats_panel(World &world, const DevToolsState &tools) noexcept;

} // namespace fr::devtools
