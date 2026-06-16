/**
 * @file material_override_panel.hpp
 * @author Tfoedy
 * @brief ImGui material override editor for selected mesh entities.
 */

#pragma once

#include <imgui.h>

#include "fr/asscooker/asscooker.hpp"
#include "fr/asscooker/dev_asset_catalog.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/logger/logger.hpp"
#include "fr/scene/render_asset_system.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr::asscooker::imgui {

struct MaterialOverridePanelState {
    char output_path[512]{"assets/materials/overrides/material_override.fmat"};

    char albedo_texture_path[512]{};
    char normal_texture_path[512]{};
    char extra_texture_path[512]{};

    F32 base_color[4]{1.0f, 1.0f, 1.0f, 1.0f};

    F32 metallic{0.0f};
    F32 roughness{1.0f};
    F32 alpha{1.0f};
    F32 alpha_cutoff{0.5f};

    S32 shading_model{static_cast<S32>(MaterialShadingModel::PBR)};
    S32 blend_mode{static_cast<S32>(MaterialBlendMode::Opaque)};

    bool force{true};
};

inline AssetId texture_id_from_path(const char *path) noexcept {
    if (!path || path[0] == '\0') {
        return {};
    }

    return AssetId::from_logical_path(StringView(path));
}

inline bool register_material_outputs(AssetRegistry &registry,
                                      Slice<const CookedAssetOutput> outputs) noexcept {
    bool ok = true;

    for (const CookedAssetOutput &output : outputs) {
        if (!output.id.is_valid() || output.kind == AssetKind::Unknown || output.path.size() == 0) {
            ok = false;
            continue;
        }

        ok = registry.register_loose_asset(output.id, output.kind, output.path.view(),
                                           output.content_hash) &&
             ok;
    }

    return ok;
}

inline MaterialShadingModel shading_model_from_s32(S32 value) noexcept {
    if (value == static_cast<S32>(MaterialShadingModel::Unlit)) {
        return MaterialShadingModel::Unlit;
    }

    if (value == static_cast<S32>(MaterialShadingModel::Standard)) {
        return MaterialShadingModel::Standard;
    }

    return MaterialShadingModel::PBR;
}

inline MaterialBlendMode blend_mode_from_s32(S32 value) noexcept {
    if (value == static_cast<S32>(MaterialBlendMode::Masked)) {
        return MaterialBlendMode::Masked;
    }

    if (value == static_cast<S32>(MaterialBlendMode::Transparent)) {
        return MaterialBlendMode::Transparent;
    }

    return MaterialBlendMode::Opaque;
}

inline void draw_material_fields(MaterialOverridePanelState &state) noexcept {
    ImGui::InputText("Output .fmat", state.output_path, sizeof(state.output_path));

    ImGui::ColorEdit4("Base Color", state.base_color);

    const char *shading_items[] = {"Unlit", "Standard", "PBR"};
    ImGui::Combo("Shading Model", &state.shading_model, shading_items, 3);

    const char *blend_items[] = {"Opaque", "Masked", "Transparent"};
    ImGui::Combo("Blend Mode", &state.blend_mode, blend_items, 3);

    ImGui::DragFloat("Metallic", &state.metallic, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Roughness", &state.roughness, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Alpha", &state.alpha, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Alpha Cutoff", &state.alpha_cutoff, 0.01f, 0.0f, 1.0f);

    ImGui::Separator();

    ImGui::InputText("Albedo .ftex", state.albedo_texture_path, sizeof(state.albedo_texture_path));
    ImGui::InputText("Normal .ftex", state.normal_texture_path, sizeof(state.normal_texture_path));
    ImGui::InputText("Extra .ftex", state.extra_texture_path, sizeof(state.extra_texture_path));

    ImGui::Checkbox("Force Cook", &state.force);
}

inline bool cook_and_apply_material_override(DevAssetImportContext &import_ctx,
                                             fr::devtools::EditorContext &editor_ctx,
                                             MaterialOverridePanelState &state,
                                             Thing thing) noexcept {
    if (!import_ctx.is_valid()) {
        FR_LOG_ERR("[MaterialOverride] Invalid import context.");
        return false;
    }

    if (state.output_path[0] == '\0') {
        FR_LOG_ERR("[MaterialOverride] Output path is empty.");
        return false;
    }

    MaterialCookDesc desc{};
    desc.albedo_texture = texture_id_from_path(state.albedo_texture_path);
    desc.normal_texture = texture_id_from_path(state.normal_texture_path);
    desc.extra_texture = texture_id_from_path(state.extra_texture_path);

    desc.base_color_factor[0] = state.base_color[0];
    desc.base_color_factor[1] = state.base_color[1];
    desc.base_color_factor[2] = state.base_color[2];
    desc.base_color_factor[3] = state.base_color[3];

    desc.metallic_factor = state.metallic;
    desc.roughness_factor = state.roughness;
    desc.alpha = state.alpha;
    desc.alpha_cutoff = state.alpha_cutoff;

    desc.shading_model = shading_model_from_s32(state.shading_model);
    desc.blend_mode = blend_mode_from_s32(state.blend_mode);

    DynamicArray<CookedAssetOutput> outputs(import_ctx.alloc);

    CookOptions options{};
    options.force = state.force;
    options.output_id = AssetId::from_logical_path(StringView(state.output_path));

    if (!cook_material_ex(desc, StringView(state.output_path), &outputs, options)) {
        FR_LOG_ERR("[MaterialOverride] Failed to cook material: {}", StringView(state.output_path));
        return false;
    }

    import_ctx.catalog->add_or_replace(outputs.slice(), StringView("material override"));

    if (!register_material_outputs(*import_ctx.registry, outputs.slice())) {
        FR_LOG_ERR("[MaterialOverride] Failed to register cooked material output.");
        return false;
    }

    if (!import_ctx.manifest_path.is_empty()) {
        if (!import_ctx.catalog->build_loose_manifest(import_ctx.manifest_path)) {
            FR_LOG_ERR("[MaterialOverride] Failed to rebuild development manifest: {}",
                       import_ctx.manifest_path);
            return false;
        }
    }

    MaterialOverridePart *override_part = editor_ctx.world->try_get<MaterialOverridePart>(thing);
    if (!override_part) {
        override_part = &editor_ctx.world->emplace_now<MaterialOverridePart>(thing);
    }

    if (override_part->material_handle.is_valid()) {
        editor_ctx.assets->unload_material(override_part->material_handle);
    }

    override_part->material_path = String::from_view(StringView(state.output_path));
    override_part->material_id = AssetId::from_logical_path(override_part->material_path.view());
    override_part->resolved_material_id = {};
    override_part->material_handle = {};

    RenderAssetSystem::resolve(*editor_ctx.world, *editor_ctx.assets);

    return true;
}

/**
 * @brief Draws material override editor for selected mesh entity.
 */
inline void material_override_panel(DevAssetImportContext &import_ctx,
                                    fr::devtools::EditorContext &editor_ctx,
                                    MaterialOverridePanelState &state) noexcept {
    FR_ASSERT(editor_ctx.is_valid(), "EditorContext must be valid");

    Thing selected = editor_ctx.tools->selected;
    if (selected.is_nil() || editor_ctx.world->is_dead(selected)) {
        ImGui::TextDisabled("No selected thing.");
        return;
    }

    MeshRendererPart *mesh = editor_ctx.world->try_get<MeshRendererPart>(selected);
    if (!mesh) {
        ImGui::TextDisabled("Selected thing has no MeshRendererPart.");
        return;
    }

    ImGui::Text("Selected Thing #%u", selected.idx());

    MaterialOverridePart *override_part = editor_ctx.world->try_get<MaterialOverridePart>(selected);
    if (override_part && override_part->material_path.size() != 0) {
        ImGui::TextWrapped("Current override: %s", override_part->material_path.c_str());
    } else {
        ImGui::TextDisabled("No material override.");
    }

    draw_material_fields(state);

    if (ImGui::Button("Cook / Apply Override")) {
        cook_and_apply_material_override(import_ctx, editor_ctx, state, selected);
    }

    ImGui::SameLine();

    if (ImGui::Button("Remove Override")) {
        if (override_part) {
            if (override_part->material_handle.is_valid()) {
                editor_ctx.assets->unload_material(override_part->material_handle);
            }

            editor_ctx.world->destroy_now<MaterialOverridePart>(selected);
        }
    }
}

} // namespace fr::asscooker::imgui
