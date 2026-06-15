/**
 * @file devtools_state.hpp
 * @author Tfoedy
 * @brief Shared runtime devtools state.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"
#include "fr/devtools/inspector_state.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/scene/render_scene_extractor.hpp"

namespace fr::devtools {

/**
 * @brief Active transform gizmo operation.
 */
enum class GizmoMode : U8 {
    Translate,
    Rotate,
    Scale,
};

/**
 * @brief Transform gizmo coordinate space.
 */
enum class GizmoSpace : U8 {
    World,
    Local,
};

/**
 * @brief Shared state for runtime-integrated development tools.
 *
 * @details
 * This state is intentionally runtime-facing. It stores selected/hovered entity state, gizmo mode
 * and renderer debug/tuning settings used by development applications.
 */
struct DevToolsState {
    InspectorState inspector{};

    Thing selected{Thing::nil()};
    Thing hovered{Thing::nil()};

    GizmoMode gizmo_mode{GizmoMode::Translate};
    GizmoSpace gizmo_space{GizmoSpace::World};

    bool enabled{true};
    bool show_inspector{true};
    bool show_render_settings{true};
    bool show_spawn_panel{true};
    bool show_gizmos{true};

    RenderDirectionalShadowSettings directional_shadow_settings{};
    RenderLightingSettings lighting{};
    RenderAmbientOcclusionSettings ao{};
    RenderIblSettings ibl{};
    RenderDebugSettings debug{};
};

/**
 * @brief Synchronizes the shared selected thing with the inspector selection.
 */
inline void set_selected_thing(DevToolsState &tools, Thing thing) noexcept {
    tools.selected = thing;
    tools.inspector.selected_thing = thing;

    if (thing.is_nil()) {
        tools.inspector.active_view = InspectorState::View::None;
        return;
    }

    tools.inspector.active_view = InspectorState::View::Thing;
}

} // namespace fr::devtools
