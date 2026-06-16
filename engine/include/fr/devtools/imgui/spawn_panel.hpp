/**
 * @file spawn_panel.hpp
 * @author Tfoedy
 * @brief Generic runtime devtools spawn and scene IO panel.
 */

#pragma once

#include <cstring>

#include <imgui.h>

#include "fr/core/typedefs.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/devtools/scene_io.hpp"
#include "fr/devtools/world_actions.hpp"
#include "fr/logger/logger.hpp"

namespace fr::devtools {

/**
 * @brief Persistent UI state for the generic spawn panel.
 */
struct SpawnPanelState {
    char mesh_path[512]{"assets/models/imported/model/model.fmesh"};
    char scene_path[512]{"scenes/demo_scene.json"};

    SpawnTransformDesc transform{};

    bool open_spawn_transform{true};
    bool open_scene_io{true};

    bool request_save_scene{false};
    bool request_load_scene{false};
};

/**
 * @brief Resets the spawn transform controls to identity.
 */
inline void reset_spawn_transform(SpawnPanelState &state) noexcept {
    state.transform = {};
}

/**
 * @brief Draws transform controls used for newly spawned entities.
 */
inline void draw_spawn_transform_controls(SpawnTransformDesc &transform) noexcept {
    ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
    ImGui::DragFloat3("Scale", &transform.scale.x, 0.05f, 0.001f, 1000.0f);
}

/**
 * @brief Draws generic entity spawn controls.
 *
 * @details
 * This panel does not import source assets. Mesh spawning expects a cooked logical .fmesh path
 * registered in AssetRegistry.
 */
inline void draw_spawn_controls(EditorContext &ctx, SpawnPanelState &state) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    if (ImGui::CollapsingHeader("Spawn Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_spawn_transform_controls(state.transform);

        if (ImGui::SmallButton("Reset Transform")) {
            reset_spawn_transform(state);
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Empty")) {
        spawn_empty(ctx, state.transform);
    }

    ImGui::SameLine();

    if (ImGui::Button("Camera")) {
        spawn_camera(ctx, state.transform);
    }

    if (ImGui::Button("Directional Light")) {
        spawn_directional_light(ctx, state.transform);
    }

    ImGui::SameLine();

    if (ImGui::Button("Point Light")) {
        spawn_point_light(ctx, state.transform);
    }

    ImGui::SameLine();

    if (ImGui::Button("Spot Light")) {
        spawn_spot_light(ctx, state.transform);
    }

    ImGui::Separator();

    ImGui::InputText("Cooked Mesh Path", state.mesh_path, sizeof(state.mesh_path));

    if (ImGui::Button("Spawn Mesh")) {
        if (state.mesh_path[0] == '\0') {
            FR_LOG_ERR("[DevTools] Cannot spawn mesh from an empty path.");
        } else {
            spawn_mesh(ctx, StringView(state.mesh_path), state.transform);
        }
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Expected a cooked logical .fmesh path registered in AssetRegistry.");
    }
}

/**
 * @brief Draws scene save/load controls.
 */
inline void draw_scene_io_controls(EditorContext &ctx, SpawnPanelState &state) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    ImGui::InputText("Scene Path", state.scene_path, sizeof(state.scene_path));

    if (ImGui::Button("Save Scene")) {
        if (state.scene_path[0] == '\0') {
            FR_LOG_ERR("[DevTools] Cannot save scene to an empty path.");
        } else {
            state.request_save_scene = true;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Load Scene")) {
        if (state.scene_path[0] == '\0') {
            FR_LOG_ERR("[DevTools] Cannot load scene from an empty path.");
        } else {
            state.request_load_scene = true;
        }
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Scene IO is deferred until after ECS systems finish this frame.");
    }
}

/**
 * @brief Draws the full generic spawn panel.
 */
inline void spawn_panel(EditorContext &ctx, SpawnPanelState &state) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_spawn_controls(ctx, state);
    }

    if (ImGui::CollapsingHeader("Scene IO", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_scene_io_controls(ctx, state);
    }
}

} // namespace fr::devtools
