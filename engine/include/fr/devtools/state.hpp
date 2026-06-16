/**
 * @file state.hpp
 * @author Kiju
 * @brief Unified devtools UI state. Uberstruct.
 */

#pragma once

#include "fr/core/meta.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/thing.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/scene/render_scene_extractor.hpp"

namespace fr::devtools {

struct DevToolsState {
    // --------------------------------------------------------------- Selection
    Thing selected{Thing::nil()};
    Thing hovered{Thing::nil()};

    // ---------------------------------------------------- Inspector Navigation
    enum class InspectorView : U8 { None, Thing, Pool, Resource };
    InspectorView inspector_view{InspectorView::None};
    TypeIdx inspector_pool{TypeIdx::nil()};
    TypeIdx inspector_resource{TypeIdx::nil()};

    // -------------------------------------------------------- Panel Visibility
    bool show_inspector{true};
    bool show_render_settings{true};
    bool show_spawn_panel{true};
    bool show_stats{true};
    bool show_assets{true};
    bool show_controls{true};

    // ----------------------------------------------------------------- Layout
    bool snap_panels{false};

    // -------------------------------------------------------- Spawn / Scene IO
    char mesh_path[512]{"assets/models/imported/model/model.fmesh"};
    char scene_path[512]{"scenes/demo_scene.json"};
    LocalTransformPart spawn_transform{};
    bool request_save_scene{false};
    bool request_load_scene{false};

    // --------------------------------------------------------- Transform Gizmo
    enum class GizmoOp : U8 { Translate, Rotate, Scale, TranslateRotate };
    enum class GizmoSpace : U8 { World, Local };
    GizmoOp gizmo_op{GizmoOp::Translate};
    GizmoSpace gizmo_space{GizmoSpace::World};
    bool gizmo_enabled{true};
    bool gizmo_snap{false};
    F32 gizmo_translate_snap{0.25f};
    F32 gizmo_rotate_snap_deg{15.0f};
    F32 gizmo_scale_snap{0.10f};

    // ---------------------------------------------------------- Renderer Debug
    S32 shading_override{0}; // cast to ShadingOverride

    // ---------------------------------------------------------- Render settings
    RenderDirectionalShadowSettings directional_shadow_settings{};
    RenderLightingSettings lighting{};
    RenderAmbientOcclusionSettings ao{};
    RenderIblSettings ibl{};
    RenderDebugSettings debug{};

    // ----------------------------- Frame Stats (transient, updated each frame)
    F32 dt{0.0f};
    F32 fps{0.0f};
    U32 viewport_width{0};
    U32 viewport_height{0};
    RenderSubmitStats geometry_stats{};
    RenderSubmitStats shadow_stats{};
    bool has_main_camera{false};

    template <typename Archive>
    void shape(Archive &ar) noexcept {
        ar.prop("lighting", lighting);
        ar.prop("ao", ao);
        ar.prop("ibl", ibl);
        ar.prop("debug", debug);
        ar.prop("directional_shadows", directional_shadow_settings);
        ar.prop("gizmo_translate_snap", gizmo_translate_snap);
        ar.prop("gizmo_rotate_snap_deg", gizmo_rotate_snap_deg);
        ar.prop("gizmo_scale_snap", gizmo_scale_snap);
        ar.prop("gizmo_snap", gizmo_snap);
    }
};

/// @brief Sets the selected thing and updates inspector navigation state.
inline bool set_selected_thing(DevToolsState &tools, Thing thing) noexcept {
    tools.selected = thing;

    if (thing.is_nil()) {
        tools.inspector_view = DevToolsState::InspectorView::None;
        return false;
    } else {
        tools.inspector_view = DevToolsState::InspectorView::Thing;
        return true;
    }
}

} // namespace fr::devtools

FR_TYPE(fr::devtools::DevToolsState);
