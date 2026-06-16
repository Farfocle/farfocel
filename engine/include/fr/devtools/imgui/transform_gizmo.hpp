/**
 * @file transform_gizmo.hpp
 * @author Tfoedy
 * @brief ImGuizmo transform manipulation helpers for runtime devtools.
 */

#pragma once

#include <ImGuizmo.h>
#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "fr/core/math.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/devtools_state.hpp"
#include "fr/devtools/object_picking.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/transform_system.hpp"

namespace fr::devtools {

enum class TransformGizmoOperation : S32 {
    Translate = 0,
    Rotate = 1,
    Scale = 2,
};

enum class TransformGizmoSpace : S32 {
    Local = 0,
    World = 1,
};

struct TransformGizmoState {
    TransformGizmoOperation operation{TransformGizmoOperation::Translate};
    TransformGizmoSpace space{TransformGizmoSpace::World};

    bool enabled{true};
    bool use_snap{false};

    F32 translate_snap{0.25f};
    F32 rotate_snap_deg{15.0f};
    F32 scale_snap{0.10f};

    template <typename Archive>
    void shape(Archive &) noexcept {
        // Runtime-only editor state. Intentionally not serialized.
    }
};

inline ImGuizmo::OPERATION imguizmo_operation(TransformGizmoOperation operation) noexcept {
    switch (operation) {
    case TransformGizmoOperation::Translate:
        return ImGuizmo::TRANSLATE;
    case TransformGizmoOperation::Rotate:
        return ImGuizmo::ROTATE;
    case TransformGizmoOperation::Scale:
        return ImGuizmo::SCALE;
    default:
        return ImGuizmo::TRANSLATE;
    }
}

inline ImGuizmo::MODE imguizmo_mode(TransformGizmoSpace space) noexcept {
    return space == TransformGizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
}

inline Mat4 compose_local_transform(const LocalTransformPart &local) noexcept {
    const Mat4 translation = glm::translate(Mat4(1.0f), local.position);
    const Mat4 rotation = glm::mat4_cast(local.rotation);
    const Mat4 scale = glm::scale(Mat4(1.0f), local.scale);

    return translation * rotation * scale;
}

inline void decompose_transform_matrix(const Mat4 &matrix, LocalTransformPart &out_local) noexcept {
    Vec3 position = Vec3(matrix[3]);

    Vec3 basis_x = Vec3(matrix[0]);
    Vec3 basis_y = Vec3(matrix[1]);
    Vec3 basis_z = Vec3(matrix[2]);

    Vec3 scale{};
    scale.x = glm::length(basis_x);
    scale.y = glm::length(basis_y);
    scale.z = glm::length(basis_z);

    if (scale.x <= 0.000001f) {
        scale.x = 1.0f;
    }

    if (scale.y <= 0.000001f) {
        scale.y = 1.0f;
    }

    if (scale.z <= 0.000001f) {
        scale.z = 1.0f;
    }

    basis_x /= scale.x;
    basis_y /= scale.y;
    basis_z /= scale.z;

    Mat3 rotation_matrix{};
    rotation_matrix[0] = basis_x;
    rotation_matrix[1] = basis_y;
    rotation_matrix[2] = basis_z;

    out_local.position = position;
    out_local.rotation = glm::quat_cast(rotation_matrix);
    out_local.scale = scale;
}

inline void apply_world_matrix_to_local(World &world, Thing thing,
                                        const Mat4 &edited_world_matrix) noexcept {
    LocalTransformPart *local = world.try_get<LocalTransformPart>(thing);
    if (!local) {
        return;
    }

    /*
        MVP path: edited world matrix is treated as local matrix.
        This is correct for root objects. Parent-aware conversion can be added later.
    */
    decompose_transform_matrix(edited_world_matrix, *local);
}

inline void draw_transform_gizmo(World &world, DevToolsState &tools, TransformGizmoState &state,
                                 F32 viewport_width, F32 viewport_height) noexcept {
    if (!state.enabled) {
        return;
    }

    if (viewport_width <= 0.0f || viewport_height <= 0.0f) {
        return;
    }

    Thing selected = tools.selected;
    if (selected.is_nil() || world.is_dead(selected)) {
        return;
    }

    LocalTransformPart *local = world.try_get<LocalTransformPart>(selected);
    WorldTransformPart *world_transform = world.try_get<WorldTransformPart>(selected);

    if (!local || !world_transform) {
        return;
    }

    const F32 aspect = viewport_width / viewport_height;
    EditorCameraMatrices camera = extract_editor_camera_matrices(world, aspect);

    if (!camera.found) {
        return;
    }

    Mat4 object_matrix = world_transform->matrix;

    ImGuizmo::SetOrthographic(false);

    /*
        Use default draw list. This avoids depending on newer ImGuizmo overloads
        and works with older ImGuizmo versions.
    */
    ImGuizmo::SetDrawlist();

    ImGuizmo::SetRect(0.0f, 0.0f, viewport_width, viewport_height);

    F32 snap_values[3] = {0.0f, 0.0f, 0.0f};
    F32 *snap = nullptr;

    if (state.use_snap) {
        switch (state.operation) {
        case TransformGizmoOperation::Translate:
            snap_values[0] = state.translate_snap;
            snap_values[1] = state.translate_snap;
            snap_values[2] = state.translate_snap;
            break;

        case TransformGizmoOperation::Rotate:
            snap_values[0] = state.rotate_snap_deg;
            snap_values[1] = state.rotate_snap_deg;
            snap_values[2] = state.rotate_snap_deg;
            break;

        case TransformGizmoOperation::Scale:
            snap_values[0] = state.scale_snap;
            snap_values[1] = state.scale_snap;
            snap_values[2] = state.scale_snap;
            break;
        }

        snap = snap_values;
    }

    ImGuizmo::Manipulate(glm::value_ptr(camera.view), glm::value_ptr(camera.projection),
                         imguizmo_operation(state.operation), imguizmo_mode(state.space),
                         glm::value_ptr(object_matrix), nullptr, snap);

    if (ImGuizmo::IsUsing()) {
        apply_world_matrix_to_local(world, selected, object_matrix);
        TransformSystem::rebuild_world_transforms(world);
    }
}

inline void draw_transform_gizmo_toolbar(TransformGizmoState &state) noexcept {
    ImGui::PushID("transform_gizmo_toolbar");

    ImGui::Checkbox("Gizmo Enabled##enabled", &state.enabled);

    S32 operation = static_cast<S32>(state.operation);
    const char *operation_items[] = {"Translate", "Rotate", "Scale"};

    if (ImGui::Combo("Operation##operation", &operation, operation_items, 3)) {
        if (operation < 0) {
            operation = 0;
        }

        if (operation > 2) {
            operation = 2;
        }

        state.operation = static_cast<TransformGizmoOperation>(operation);
    }

    S32 space = static_cast<S32>(state.space);
    const char *space_items[] = {"Local", "World"};

    if (ImGui::Combo("Space##space", &space, space_items, 2)) {
        if (space < 0) {
            space = 0;
        }

        if (space > 1) {
            space = 1;
        }

        state.space = static_cast<TransformGizmoSpace>(space);
    }

    ImGui::Checkbox("Snap##snap", &state.use_snap);

    if (state.use_snap) {
        ImGui::DragFloat("Translate Snap##translate_snap", &state.translate_snap, 0.01f, 0.001f,
                         100.0f);
        ImGui::DragFloat("Rotate Snap##rotate_snap", &state.rotate_snap_deg, 1.0f, 0.1f, 180.0f);
        ImGui::DragFloat("Scale Snap##scale_snap", &state.scale_snap, 0.01f, 0.001f, 100.0f);
    }

    ImGui::TextDisabled("Shortcuts: W translate, E rotate, R scale, T local/world");

    ImGui::PopID();
}

inline void update_transform_gizmo_shortcuts(TransformGizmoState &state) noexcept {
    ImGuiIO &io = ImGui::GetIO();

    if (io.WantCaptureKeyboard) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        state.operation = TransformGizmoOperation::Translate;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        state.operation = TransformGizmoOperation::Rotate;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        state.operation = TransformGizmoOperation::Scale;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_T, false)) {
        state.space = state.space == TransformGizmoSpace::Local ? TransformGizmoSpace::World
                                                                : TransformGizmoSpace::Local;
    }
}

} // namespace fr::devtools
