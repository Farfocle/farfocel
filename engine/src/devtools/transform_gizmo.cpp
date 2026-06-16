/**
 * @file transform_gizmo.cpp
 * @brief ImGuizmo transform manipulation helpers for runtime devtools.
 */

#include "fr/devtools/gizmo.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "fr/data/parts.hpp"
#include "fr/scene/picking.hpp"

namespace fr::devtools {

static ImGuizmo::OPERATION to_imguizmo_operation(DevToolsState::GizmoOp op) noexcept {
    switch (op) {
    case DevToolsState::GizmoOp::Rotate:          return ImGuizmo::ROTATE;
    case DevToolsState::GizmoOp::Scale:           return ImGuizmo::SCALE;
    case DevToolsState::GizmoOp::TranslateRotate: return static_cast<ImGuizmo::OPERATION>(ImGuizmo::TRANSLATE | ImGuizmo::ROTATE);
    default:                                       return ImGuizmo::TRANSLATE;
    }
}

static ImGuizmo::MODE to_imguizmo_mode(DevToolsState::GizmoSpace space) noexcept {
    return space == DevToolsState::GizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
}

void draw_transform_gizmo(World &world, DevToolsState &tools,
                          F32 viewport_width, F32 viewport_height) noexcept {
    if (!tools.gizmo_enabled || viewport_width <= 0.0f || viewport_height <= 0.0f) {
        return;
    }

    const Thing selected = tools.selected;
    if (selected.is_nil() || world.is_dead(selected)) {
        return;
    }

    LocalTransformPart *local = world.try_get<LocalTransformPart>(selected);
    WorldTransformPart *world_transform = world.try_get<WorldTransformPart>(selected);

    if (!local || !world_transform) {
        return;
    }

    const CameraMatrices camera =
        extract_camera_matrices(world, viewport_width / viewport_height);
    if (!camera.found) {
        return;
    }

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0.0f, 0.0f, viewport_width, viewport_height);

    Mat4 model = world_transform->matrix;

    const ImGuizmo::OPERATION op = to_imguizmo_operation(tools.gizmo_op);
    const ImGuizmo::MODE mode = to_imguizmo_mode(tools.gizmo_space);

    F32 snap_value = 0.0f;
    if (tools.gizmo_snap) {
        switch (tools.gizmo_op) {
        case DevToolsState::GizmoOp::Translate:
        case DevToolsState::GizmoOp::TranslateRotate:
            snap_value = tools.gizmo_translate_snap;
            break;
        case DevToolsState::GizmoOp::Rotate:
            snap_value = tools.gizmo_rotate_snap_deg;
            break;
        case DevToolsState::GizmoOp::Scale:
            snap_value = tools.gizmo_scale_snap;
            break;
        }
    }

    const F32 snap[3] = {snap_value, snap_value, snap_value};

    bool manipulated =
        ImGuizmo::Manipulate(glm::value_ptr(camera.view), glm::value_ptr(camera.projection), op,
                             mode, glm::value_ptr(model), nullptr,
                             tools.gizmo_snap ? snap : nullptr);

    if (manipulated) {
        Vec3 position{};
        Vec3 rotation_euler{};
        Vec3 scale{};
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), glm::value_ptr(position),
                                              glm::value_ptr(rotation_euler),
                                              glm::value_ptr(scale));

        local->position = position;
        local->rotation = glm::quat(glm::radians(rotation_euler));
        local->scale = scale;

        world_transform->position = position;
        world_transform->rotation = local->rotation;
        world_transform->scale = scale;
        world_transform->matrix = model;
    }

}

void draw_transform_gizmo_toolbar(DevToolsState &tools) noexcept {
    ImGui::Checkbox("Gizmo##gizmo_enabled", &tools.gizmo_enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Snap##gizmo_snap", &tools.gizmo_snap);

    const char *op_names[] = {"Translate", "Rotate", "Scale", "Translate+Rotate"};
    S32 op = static_cast<S32>(tools.gizmo_op);
    if (ImGui::Combo("Operation##gizmo_op", &op, op_names, 4)) {
        if (op >= 0 && op <= 3) {
            tools.gizmo_op = static_cast<DevToolsState::GizmoOp>(op);
        }
    }

    const char *space_names[] = {"World", "Local"};
    S32 space = static_cast<S32>(tools.gizmo_space);
    if (ImGui::Combo("Space##gizmo_space", &space, space_names, 2)) {
        if (space >= 0 && space <= 1) {
            tools.gizmo_space = static_cast<DevToolsState::GizmoSpace>(space);
        }
    }

    if (tools.gizmo_snap) {
        ImGui::DragFloat("Translate Snap##gizmo_tsnap", &tools.gizmo_translate_snap, 0.05f,
                         0.01f, 10.0f);
        ImGui::DragFloat("Rotate Snap°##gizmo_rsnap", &tools.gizmo_rotate_snap_deg, 1.0f,
                         1.0f, 90.0f);
        ImGui::DragFloat("Scale Snap##gizmo_ssnap", &tools.gizmo_scale_snap, 0.01f, 0.01f,
                         1.0f);
    }
}

void update_transform_gizmo_shortcuts(DevToolsState &tools) noexcept {
    if (ImGui::IsKeyPressed(ImGuiKey_T, false)) {
        tools.gizmo_op = DevToolsState::GizmoOp::Translate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        tools.gizmo_op = DevToolsState::GizmoOp::Rotate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        tools.gizmo_op = DevToolsState::GizmoOp::Scale;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        tools.gizmo_op = DevToolsState::GizmoOp::TranslateRotate;
    }
}

} // namespace fr::devtools
