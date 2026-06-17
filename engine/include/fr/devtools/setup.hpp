/**
 * @file setup.hpp
 * @brief One-call devtools world setup: resources, systems, frame helpers.
 *
 * @details
 * Include this header and call setup_devtools_world() to get a fully wired devtools
 * environment. The demo then only needs to set up the scene itself.
 */

#pragma once

// clang-format off
#include <imgui.h>
#include <ImGuizmo.h>
// clang-format on

#include <glm/gtc/quaternion.hpp>

#include "fr/asscooker/dev_asset_catalog.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/asset_panel.hpp"
#include "fr/devtools/gizmo.hpp"
#include "fr/devtools/render_settings_panel.hpp"
#include "fr/devtools/renderer_inspector.hpp"
#include "fr/devtools/spawn_panel.hpp"
#include "fr/devtools/state.hpp"
#include "fr/devtools/stats_panel.hpp"
#include "fr/devtools/world_actions.hpp"
#include "fr/devtools/world_inspector.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/keycode.hpp"
#include "fr/scene/app.hpp"
#include "fr/scene/app_state.hpp"
#include "fr/scene/environment.hpp"
#include "fr/scene/picking.hpp"
#include "fr/scene/render_assets.hpp"
#include "fr/scene/render_extractor.hpp"
#include "fr/scene/scene_io.hpp"
#include "fr/scene/scene_render_settings.hpp"

namespace fr::devtools {

// =========================================================== Systems (inline)

/// @brief FPS camera controller system. Handles RMB-activated mouse look + WASD movement.
inline void devtools_fps_camera_system(fr::Scope scope) noexcept {
    auto &ctx = scope.get_resource<fr::scene::AppState>();
    auto &input = scope.get_resource<fr::WindowInput>();
    ImGuiIO &io = ImGui::GetIO();

    const bool should_be_active =
        ctx.window->is_focused() && input.is_mouse_down(ctx.camera_button) && !io.WantCaptureMouse;

    if (should_be_active != ctx.camera_active) {
        ctx.camera_active = should_be_active;
        ctx.window->set_mouse_mode(ctx.camera_active ? fr::MouseMode::Relative
                                                     : fr::MouseMode::Normal);
    }

    if (!ctx.camera_active) {
        return;
    }

    const bool precision = input.is_key_down(fr::Key::LShift);

    for (auto [thing, fps, local] : scope.query<fr::FPSControllerPart, fr::LocalTransformPart>()) {
        (void)thing;

        fps.yaw -= input.mouse_delta_x * fps.mouse_sensitivity;
        fps.pitch -= input.mouse_delta_y * fps.mouse_sensitivity;
        fps.pitch = glm::clamp(fps.pitch, -89.0f, 89.0f);

        local.rotation = glm::quat(glm::vec3(glm::radians(fps.pitch), glm::radians(fps.yaw), 0.0f));

        const fr::Vec3 forward = local.rotation * fr::Vec3(0.0f, 0.0f, -1.0f);
        const fr::Vec3 right = local.rotation * fr::Vec3(1.0f, 0.0f, 0.0f);
        const fr::Vec3 up = fr::Vec3(0.0f, 1.0f, 0.0f);

        F32 speed = fps.move_speed * ctx.dt;
        if (precision)
            speed *= 0.15f;

        if (input.is_key_down(fr::Key::W))
            local.position += forward * speed;
        if (input.is_key_down(fr::Key::S))
            local.position -= forward * speed;
        if (input.is_key_down(fr::Key::A))
            local.position -= right * speed;
        if (input.is_key_down(fr::Key::D))
            local.position += right * speed;
        if (input.is_key_down(fr::Key::Space))
            local.position += up * speed;
    }

    fr::rebuild_world_transforms(scope.world());
}

/// @brief Draws the Controls panel: panel toggles, gizmo mode, camera help, layout actions.
inline void draw_controls_panel(DevToolsState &tools) noexcept {
    // --------------------------------------------------------- Panels
    ImGui::SeparatorText("Panels");

    auto panel_row = [](const char *shortcut, const char *label, bool *flag) {
        ImGui::TextDisabled("%s", shortcut);
        ImGui::SameLine();
        ImGui::Checkbox(label, flag);
    };
    panel_row("[1]", "World Inspector##ctl", &tools.show_inspector);
    panel_row("[2]", "Spawn / Scene##ctl", &tools.show_spawn_panel);
    panel_row("[3]", "Renderer##ctl", &tools.show_render_settings);
    panel_row("[4]", "Stats##ctl", &tools.show_stats);
    panel_row("[5]", "Assets / Materials##ctl", &tools.show_assets);

    // --------------------------------------------------------- Gizmo
    ImGui::SeparatorText("Gizmo");
    ImGui::Checkbox("Enabled##ctl_gizmo", &tools.gizmo_enabled);
    ImGui::SameLine();

    auto gizmo_btn = [&](const char *label, DevToolsState::GizmoOp op) {
        const bool active = tools.gizmo_op == op && tools.gizmo_enabled;
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label) && tools.gizmo_enabled)
            tools.gizmo_op = op;
        if (active)
            ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    gizmo_btn("T##op_t", DevToolsState::GizmoOp::Translate);
    gizmo_btn("R##op_r", DevToolsState::GizmoOp::Rotate);
    gizmo_btn("E##op_e", DevToolsState::GizmoOp::TranslateRotate);
    gizmo_btn("Y##op_y", DevToolsState::GizmoOp::Scale);
    ImGui::NewLine();

    const char *space_names[] = {"World", "Local"};
    S32 space = static_cast<S32>(tools.gizmo_space);
    if (ImGui::Combo("Space##ctl_space", &space, space_names, 2)) {
        tools.gizmo_space = static_cast<DevToolsState::GizmoSpace>(space);
    }

    // --------------------------------------------------------- Camera
    ImGui::SeparatorText("Camera");
    ImGui::TextDisabled("Hold RMB — fly cam");
    ImGui::TextDisabled("WASD — move   Shift — precision");
    ImGui::TextDisabled("Space — ascend");

    // --------------------------------------------------------- Layout
    ImGui::SeparatorText("Layout");
    if (ImGui::Button("Snap All Panels [Tab]"))
        tools.snap_panels = true;
}

/// @brief Devtools panel system: keyboard shortcuts + all panel windows.
inline void devtools_panels_system(fr::Scope scope) noexcept {
    auto &state = scope.get_resource<fr::scene::AppState>();
    auto &tools = scope.get_resource<DevToolsState>();
    auto &scene_settings = scope.get_resource<fr::SceneRenderSettings>();
    auto &asset_state = scope.get_resource<AssetPanelState>();
    auto &asset_panel_ctx = scope.get_resource<AssetPanelCtx>();
    auto &input = scope.get_resource<fr::WindowInput>();
    fr::World &world = scope.world();

    // --------------------------------------------------------- Keyboard shortcuts
    // Use WantTextInput (not WantCaptureKeyboard) so shortcuts work when a panel is
    // focused/hovered but not when the user is actually typing in a text field.
    if (!ImGui::GetIO().WantTextInput) {
        if (input.is_key_pressed(fr::Key::Num1))
            tools.show_inspector = !tools.show_inspector;
        if (input.is_key_pressed(fr::Key::Num2))
            tools.show_spawn_panel = !tools.show_spawn_panel;
        if (input.is_key_pressed(fr::Key::Num3))
            tools.show_render_settings = !tools.show_render_settings;
        if (input.is_key_pressed(fr::Key::Num4))
            tools.show_stats = !tools.show_stats;
        if (input.is_key_pressed(fr::Key::Num5))
            tools.show_assets = !tools.show_assets;
        if (input.is_key_pressed(fr::Key::Num6))
            tools.show_controls = !tools.show_controls;
        if (input.is_key_pressed(fr::Key::Tab))
            tools.snap_panels = true; // one-shot
    }

    // --------------------------------------------------------- Layout
    const F32 vw = static_cast<F32>(tools.viewport_width);
    const F32 vh = static_cast<F32>(tools.viewport_height);
    const bool snap = tools.snap_panels;

    // place() applies position+size: ImGuiCond_Always on snap frame, FirstUseEver otherwise.
    auto place = [&](ImVec2 pos, ImVec2 size) {
        const ImGuiCond c = snap ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
        ImGui::SetNextWindowSize(size, c);
        ImGui::SetNextWindowPos(pos, c);
    };

    constexpr F32 PAD = 20.0f;
    constexpr F32 LEFT_W = 420.0f;
    constexpr F32 RIGHT_W = 400.0f;
    constexpr F32 BOT_H = 280.0f; // stats
    constexpr F32 TOP_H = 320.0f; // spawn/scene

    // Left column: Inspector on top, Stats pinned to bottom (no overlap).
    const F32 insp_h = vh - BOT_H - 3.0f * PAD; // gap between inspector bottom and stats top

    // --------------------------------------------------------- World Inspector
    if (tools.show_inspector) {
        place(ImVec2(PAD, PAD), ImVec2(LEFT_W, insp_h));
        ImGui::PushID("world_inspector");
        if (ImGui::Begin("World Inspector##world_insp", &tools.show_inspector)) {
            draw_world_inspector(world, tools, state.assets);
        }
        ImGui::End();
        ImGui::PopID();
    }

    // --------------------------------------------------------- Stats
    if (tools.show_stats) {
        place(ImVec2(PAD, vh - BOT_H - PAD), ImVec2(LEFT_W, BOT_H));
        ImGui::PushID("stats");
        if (ImGui::Begin("Stats##stats", &tools.show_stats)) {
            stats_panel(world, tools);
        }
        ImGui::End();
        ImGui::PopID();
    }

    // Right column: Spawn/Scene on top, Renderer below (no overlap).
    const F32 rend_h = vh - TOP_H - 3.0f * PAD;

    // --------------------------------------------------------- Spawn / Scene
    if (tools.show_spawn_panel) {
        place(ImVec2(vw - RIGHT_W - PAD, PAD), ImVec2(RIGHT_W, TOP_H));
        ImGui::PushID("spawn_scene");
        if (ImGui::Begin("Spawn / Scene##spawn_scene", &tools.show_spawn_panel)) {
            draw_spawn_panel(world, tools);
        }
        ImGui::End();
        ImGui::PopID();
    }

    // --------------------------------------------------------- Renderer
    if (tools.show_render_settings) {
        place(ImVec2(vw - RIGHT_W - PAD, TOP_H + 2.0f * PAD), ImVec2(RIGHT_W, rend_h));
        ImGui::PushID("renderer");
        if (ImGui::Begin("Renderer##renderer", &tools.show_render_settings)) {
            draw_renderer_settings_panel(scene_settings);
            ImGui::SeparatorText("Material Debug");
            draw_shading_override_combo(tools);
            ImGui::SeparatorText("Transform Gizmo");
            draw_transform_gizmo_toolbar(tools);
        }
        ImGui::End();
        ImGui::PopID();
    }

    // --------------------------------------------------------- Controls (center top)
    constexpr F32 CTRL_W = 320.0f;
    constexpr F32 CTRL_H = 240.0f;
    const F32 center_x = LEFT_W + 2.0f * PAD;
    const F32 center_w = vw - LEFT_W - RIGHT_W - 4.0f * PAD;

    if (tools.show_controls) {
        place(ImVec2(center_x + (center_w - CTRL_W) * 0.5f, PAD), ImVec2(CTRL_W, CTRL_H));
        ImGui::PushID("controls");
        if (ImGui::Begin("Controls##controls", &tools.show_controls)) {
            draw_controls_panel(tools);
        }
        ImGui::End();
        ImGui::PopID();
    }

    // --------------------------------------------------------- Assets (center column)
    if (tools.show_assets) {
        const F32 assets_y = tools.show_controls ? CTRL_H + 2.0f * PAD : PAD;
        const F32 assets_h = vh - assets_y - PAD;
        place(ImVec2(center_x, assets_y), ImVec2(center_w, assets_h));
        ImGui::PushID("assets");
        if (ImGui::Begin("Assets / Materials##assets", &tools.show_assets)) {
            draw_asset_panel(asset_panel_ctx, world, tools, asset_state);
        }
        ImGui::End();
        ImGui::PopID();
    }

    // Reset snap flag — Tab is a one-shot action, not a sticky mode.
    tools.snap_panels = false;
}

// =========================================================== Frame helpers

/// @brief Processes pending scene save/load requests from DevToolsState.
inline void process_scene_io(fr::World &world) noexcept {
    auto &tools = world.get_resource<DevToolsState>();
    auto &state = world.get_resource<fr::scene::AppState>();

    if (tools.request_save_scene) {
        tools.request_save_scene = false;
        if (tools.scene_path[0] != '\0') {
            fr::scene::save_scene(world, fr::StringView(tools.scene_path));
        }
    }

    if (tools.request_load_scene) {
        tools.request_load_scene = false;
        if (tools.scene_path[0] != '\0') {
            if (fr::scene::load_scene(world, *state.assets, fr::StringView(tools.scene_path),
                                      state.registry)) {
                clear_selection(tools);
            }
        }
    }
}

/// @brief Extracts the scene, applies devtools overrides, renders, draws gizmo and handles picking.
inline void draw(fr::World &world) noexcept {
    auto &state = world.get_resource<fr::scene::AppState>();
    auto &tools = world.get_resource<DevToolsState>();
    auto &input = world.get_resource<fr::WindowInput>();
    auto &scene_settings = world.get_resource<fr::SceneRenderSettings>();

    const U32 width = state.window->get_width();
    const U32 height = state.window->get_height();
    const F32 aspect = static_cast<F32>(width) / static_cast<F32>(height);

    fr::RenderExtractDesc extract_desc{};
    extract_desc.aspect_ratio = aspect;
    extract_desc.geometry_pipeline = state.renderer->geometry_pipeline(false);
    extract_desc.forward_transparent_pipeline = state.renderer->forward_transparent_pipeline();
    extract_desc.shadow_pipeline = state.renderer->shadow_pipeline();
    extract_desc.directional_shadow_settings = scene_settings.directional_shadow_settings;

    fr::RenderExtractResult result =
        fr::extract_render_frame(world, *state.assets, extract_desc, *state.submission);

    apply_shading_override(*state.submission, static_cast<ShadingOverride>(tools.shading_override));

    tools.geometry_stats = result.geometry_stats;
    tools.shadow_stats = result.shadow_stats;
    tools.has_main_camera = result.has_main_camera;

    fr::RenderFrameDesc frame_desc{};
    frame_desc.submission = state.submission;
    frame_desc.viewport.width = width;
    frame_desc.viewport.height = height;
    frame_desc.camera = result.camera;
    frame_desc.environment_source = fr::get_active_environment_texture(world, *state.assets);
    frame_desc.lighting = scene_settings.lighting;
    frame_desc.ao = scene_settings.ao;
    frame_desc.ibl = scene_settings.ibl;
    frame_desc.debug = scene_settings.debug;

    state.renderer->render(frame_desc);

    draw_transform_gizmo(world, tools, static_cast<F32>(width), static_cast<F32>(height));

    // Object picking (only when camera not active)
    if (!state.camera_active && state.window->is_focused() && !ImGui::GetIO().WantCaptureMouse &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemHovered() &&
        !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && input.is_mouse_pressed(state.pick_button)) {

        fr::CameraMatrices cam = fr::extract_camera_matrices(world, aspect);
        if (cam.found) {
            fr::PickingRay ray =
                fr::make_picking_ray(input.mouse_x, input.mouse_y, static_cast<F32>(width),
                                     static_cast<F32>(height), cam);
            fr::PickingHit hit = fr::pick_scene_mesh_aabbs(world, *state.assets, ray);
            set_selected_thing(tools, hit.is_valid() ? hit.thing : fr::Thing::nil());
        }
    }
}

/// @brief Wires up all devtools resources and schedules camera + panel systems.
inline void setup_devtools_world(fr::World &world, fr::scene::RendererApp &app,
                                 fr::asscooker::DevAssetCatalog *catalog = nullptr) noexcept {
    auto &state = world.emplace_resource<fr::scene::AppState>();
    state.window = &app.window;
    state.assets = app.assets;
    state.registry = app.registry;
    state.alloc = app.alloc;
    state.renderer = app.renderer;
    state.submission = app.submission;

    auto &panel_ctx = world.emplace_resource<AssetPanelCtx>();
    panel_ctx.alloc = app.alloc;
    panel_ctx.registry = app.registry;
    panel_ctx.assets = app.assets;
    panel_ctx.catalog = catalog;

    world.emplace_resource<DevToolsState>();
    world.emplace_resource<AssetPanelState>();
    world.emplace_resource<fr::SceneRenderSettings>();
    world.emplace_resource<fr::WindowInput>();
    world.emplace_resource<fr::EnvironmentState>();

    world.schedule(fr::Stage::Update, devtools_fps_camera_system);
    world.schedule(fr::Stage::Update, devtools_panels_system);
}

} // namespace fr::devtools
