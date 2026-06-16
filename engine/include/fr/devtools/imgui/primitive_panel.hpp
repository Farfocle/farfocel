/**
 * @file primitive_panel.hpp
 * @author Tfoedy
 * @brief ImGui panel for spawning and editing runtime primitive meshes.
 */

#pragma once

#include <imgui.h>

#include "fr/core/typedefs.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/devtools/world_actions.hpp"
#include "fr/scene/primitive_mesh_system.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr::devtools {

struct PrimitivePanelState {
    F32 size{1.0f};
    S32 x_segments{16};
    S32 z_segments{16};
    bool casts_shadow{true};
};

inline void apply_primitive_panel_defaults(PrimitiveMeshPart &primitive,
                                           const PrimitivePanelState &state) noexcept {
    primitive.size = state.size > 0.001f ? state.size : 0.001f;
    primitive.x_segments = state.x_segments > 0 ? static_cast<U32>(state.x_segments) : 1;
    primitive.z_segments = state.z_segments > 0 ? static_cast<U32>(state.z_segments) : 1;
    primitive.casts_shadow = state.casts_shadow;
}

/**
 * @brief Draws primitive spawn controls.
 */
inline void primitive_spawn_panel(EditorContext &ctx, PrimitivePanelState &state,
                                  const SpawnTransformDesc &transform) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    ImGui::DragFloat("Primitive Size", &state.size, 0.05f, 0.001f, 1000.0f);
    ImGui::DragInt("Grid X Segments", &state.x_segments, 1.0f, 1, 512);
    ImGui::DragInt("Grid Z Segments", &state.z_segments, 1.0f, 1, 512);
    ImGui::Checkbox("Casts Shadow", &state.casts_shadow);

    if (ImGui::Button("Cube")) {
        Thing thing = spawn_cube(ctx, transform);

        if (PrimitiveMeshPart *primitive = ctx.world->try_get<PrimitiveMeshPart>(thing)) {
            apply_primitive_panel_defaults(*primitive, state);
            PrimitiveMeshSystem::resolve(*ctx.world, *ctx.assets, get_ambient_ctx().alloc);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Plane")) {
        Thing thing = spawn_plane(ctx, transform);

        if (PrimitiveMeshPart *primitive = ctx.world->try_get<PrimitiveMeshPart>(thing)) {
            apply_primitive_panel_defaults(*primitive, state);
            PrimitiveMeshSystem::resolve(*ctx.world, *ctx.assets, get_ambient_ctx().alloc);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Grid")) {
        Thing thing = spawn_grid(ctx, transform);

        if (PrimitiveMeshPart *primitive = ctx.world->try_get<PrimitiveMeshPart>(thing)) {
            apply_primitive_panel_defaults(*primitive, state);
            PrimitiveMeshSystem::resolve(*ctx.world, *ctx.assets, get_ambient_ctx().alloc);
        }
    }
}

/**
 * @brief Draws editor controls for selected primitive entity.
 */
inline void selected_primitive_panel(EditorContext &ctx) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    Thing selected = ctx.tools->selected;
    if (selected.is_nil() || ctx.world->is_dead(selected)) {
        ImGui::TextDisabled("No selected thing.");
        return;
    }

    PrimitiveMeshPart *primitive = ctx.world->try_get<PrimitiveMeshPart>(selected);
    if (!primitive) {
        ImGui::TextDisabled("Selected thing has no PrimitiveMeshPart.");
        return;
    }

    bool changed = false;

    S32 kind = static_cast<S32>(primitive->kind);
    const char *items[] = {"Cube", "Plane", "Grid"};
    if (ImGui::Combo("Kind", &kind, items, 3)) {
        if (kind < 0) {
            kind = 0;
        }

        if (kind > 2) {
            kind = 2;
        }

        primitive->kind = static_cast<U32>(kind);
        changed = true;
    }

    changed = ImGui::DragFloat("Size", &primitive->size, 0.05f, 0.001f, 1000.0f) || changed;

    S32 x_segments = static_cast<S32>(primitive->x_segments);
    S32 z_segments = static_cast<S32>(primitive->z_segments);

    changed = ImGui::DragInt("Grid X Segments", &x_segments, 1.0f, 1, 512) || changed;
    changed = ImGui::DragInt("Grid Z Segments", &z_segments, 1.0f, 1, 512) || changed;

    primitive->x_segments = x_segments > 0 ? static_cast<U32>(x_segments) : 1;
    primitive->z_segments = z_segments > 0 ? static_cast<U32>(z_segments) : 1;

    changed = ImGui::Checkbox("Casts Shadow", &primitive->casts_shadow) || changed;

    if (changed) {
        PrimitiveMeshSystem::resolve(*ctx.world, *ctx.assets, get_ambient_ctx().alloc);
    }
}

} // namespace fr::devtools
