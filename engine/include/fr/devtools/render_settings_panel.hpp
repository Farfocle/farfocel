/**
 * @file render_settings_panel.hpp
 * @brief ImGui renderer settings panel for runtime devtools.
 */

#pragma once

#include "fr/devtools/state.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/scene/render_scene_extractor.hpp"

namespace fr::devtools {

const char *render_debug_mode_name(RenderDebugMode mode) noexcept;
void draw_debug_mode_combo(RenderDebugSettings &debug) noexcept;
void draw_lighting_settings(RenderLightingSettings &lighting) noexcept;
void draw_ao_settings(RenderAmbientOcclusionSettings &ao) noexcept;
void draw_ibl_settings(RenderIblSettings &ibl) noexcept;
void draw_directional_shadow_settings(RenderDirectionalShadowSettings &settings) noexcept;
void draw_renderer_settings_panel(DevToolsState &tools) noexcept;

} // namespace fr::devtools
