/**
 * @file asset_panel.hpp
 * @author Kiju
 * @brief Unified asset operations devtools panel: glTF import, material override, HDR environment.
 *
 * @details Replaces the old split of DevAssetImportContext + EnvironmentPanelContext +
 * AssetInspectorState / GltfImportPanelState / MaterialOverridePanelState / EnvironmentPanelState.
 * One context, one state, one entry point.
 */

#pragma once

#include <cstring>

#include <imgui.h>

#include "fr/asscooker/asscooker.hpp"
#include "fr/asscooker/dev_asset_catalog.hpp"
#include "fr/asscooker/dev_asset_importer.hpp"
#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_manager.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/asset/material_format.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/state.hpp"
#include "fr/devtools/world_actions.hpp"
#include "fr/logger/logger.hpp"
#include "fr/scene/environment.hpp"
#include "fr/scene/render_assets.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr::devtools {

// =============================================================================
// AssetPanelCtx
// =============================================================================

/// @brief Infrastructure context for the asset panel. Stable, non-owning pointers only.
/// Replaces both DevAssetImportContext and EnvironmentPanelContext.
struct AssetPanelCtx {
    Alloc *alloc{nullptr};
    AssetRegistry *registry{nullptr};
    AssetManager *assets{nullptr};
    asscooker::DevAssetCatalog *catalog{nullptr};

    [[nodiscard]] bool is_valid() const noexcept {
        return alloc != nullptr && registry != nullptr && assets != nullptr && catalog != nullptr;
    }

    template <typename Archive>
    void shape(Archive &) noexcept {}

    /// @brief Builds a DevAssetImportContext using paths from AssetPanelState.
    [[nodiscard]] asscooker::DevAssetImportContext
    make_import_ctx(StringView cooked_root, StringView manifest_path) const noexcept {
        asscooker::DevAssetImportContext ctx{};
        ctx.alloc = alloc;
        ctx.registry = registry;
        ctx.catalog = catalog;
        ctx.cooked_root = cooked_root;
        ctx.manifest_path = manifest_path;
        return ctx;
    }
};

// =============================================================================
// AssetPanelState
// =============================================================================

/// @brief Unified persistent UI state for all asset operations. shape()-serializable.
struct AssetPanelState {
    // ---- glTF Import --------------------------------------------------------
    char gltf_source[512]{};
    char gltf_name[128]{};
    char cooked_root[256]{"assets"};
    char manifest_path[256]{"assets/dev.fmanifest"};
    bool gltf_force{true};
    bool gltf_spawn_after{true};

    // ---- Material Override --------------------------------------------------
    char mat_output[512]{"assets/materials/overrides/material_override.fmat"};
    char mat_albedo[512]{};
    char mat_normal[512]{};
    char mat_extra[512]{};
    F32 mat_base_color[4]{1.0f, 1.0f, 1.0f, 1.0f};
    F32 mat_metallic{0.0f};
    F32 mat_roughness{1.0f};
    F32 mat_alpha{1.0f};
    F32 mat_alpha_cutoff{0.5f};
    S32 mat_shading_model{static_cast<S32>(MaterialShadingModel::PBR)};
    S32 mat_blend_mode{static_cast<S32>(MaterialBlendMode::Opaque)};
    bool mat_force{true};

    // ---- Environment HDR ----------------------------------------------------
    char env_source[512]{};
    char env_output[512]{"assets/environments/environment.ftex"};
    bool env_force{true};

    // ---- Transient status (not serialized) ----------------------------------
    char _gltf_last_mesh[512]{};
    char _gltf_last_error[256]{};
    USize _gltf_last_count{0};
    bool _gltf_last_ok{false};

    // ---- Shape protocol (serializable settings only) ------------------------
    template <typename Archive>
    void shape(Archive &ar) noexcept {
        ar.prop("gltf_force", gltf_force);
        ar.prop("gltf_spawn_after", gltf_spawn_after);

        ar.prop("mat_base_r", mat_base_color[0]);
        ar.prop("mat_base_g", mat_base_color[1]);
        ar.prop("mat_base_b", mat_base_color[2]);
        ar.prop("mat_base_a", mat_base_color[3]);
        ar.prop("mat_metallic", mat_metallic);
        ar.prop("mat_roughness", mat_roughness);
        ar.prop("mat_alpha", mat_alpha);
        ar.prop("mat_alpha_cutoff", mat_alpha_cutoff);
        ar.prop("mat_shading_model", mat_shading_model);
        ar.prop("mat_blend_mode", mat_blend_mode);
        ar.prop("mat_force", mat_force);

        ar.prop("env_force", env_force);
    }
};

// =============================================================================
// Internal tab helpers
// =============================================================================

namespace impl {

inline void str_copy(char *dst, USize dst_size, StringView src) noexcept {
    if (!dst || dst_size == 0) {
        return;
    }
    const USize n = src.size() < dst_size - 1 ? src.size() : dst_size - 1;
    if (n > 0) {
        std::memcpy(dst, src.data(), n);
    }
    dst[n] = '\0';
}

// ---- glTF Import tab -------------------------------------------------------

inline void draw_gltf_tab(AssetPanelCtx &ctx, World &world, DevToolsState &tools,
                          AssetPanelState &s) noexcept {
    ImGui::InputText("Source glTF/GLB##gltf_src", s.gltf_source, sizeof(s.gltf_source));
    ImGui::InputText("Import Name##gltf_name", s.gltf_name, sizeof(s.gltf_name));
    ImGui::InputText("Cooked Root##cooked_root", s.cooked_root, sizeof(s.cooked_root));
    ImGui::InputText("Manifest Path##manifest_path", s.manifest_path, sizeof(s.manifest_path));
    ImGui::Checkbox("Force Recook##gltf_force", &s.gltf_force);
    ImGui::Checkbox("Spawn After Import##gltf_spawn", &s.gltf_spawn_after);

    ImGui::Separator();

    if (ImGui::Button("Import glTF##gltf_import_btn")) {
        s._gltf_last_ok = false;
        s._gltf_last_mesh[0] = '\0';
        s._gltf_last_error[0] = '\0';
        s._gltf_last_count = 0;

        if (s.gltf_source[0] == '\0') {
            str_copy(s._gltf_last_error, sizeof(s._gltf_last_error),
                     StringView("Source path is empty."));
        } else if (!ctx.is_valid()) {
            str_copy(s._gltf_last_error, sizeof(s._gltf_last_error),
                     StringView("Invalid AssetPanelCtx."));
        } else {
            asscooker::DevAssetImportContext import_ctx =
                ctx.make_import_ctx(StringView(s.cooked_root), StringView(s.manifest_path));

            const StringView source(s.gltf_source);
            const StringView name = s.gltf_name[0] != '\0' ? StringView(s.gltf_name) : StringView{};

            asscooker::ImportedModelResult result =
                asscooker::import_gltf_model(import_ctx, source, name, s.gltf_force);

            if (!result.ok) {
                str_copy(s._gltf_last_error, sizeof(s._gltf_last_error),
                         StringView("import_gltf_model() failed — see log."));
            } else {
                s._gltf_last_ok = true;
                s._gltf_last_count = result.output_count;
                str_copy(s._gltf_last_mesh, sizeof(s._gltf_last_mesh), result.mesh_path.view());

                if (s.gltf_spawn_after) {
                    spawn_mesh(world, tools, result.mesh_path.view());
                }
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear Status##gltf_clear")) {
        s._gltf_last_ok = false;
        s._gltf_last_mesh[0] = '\0';
        s._gltf_last_error[0] = '\0';
        s._gltf_last_count = 0;
    }

    ImGui::Separator();

    if (s._gltf_last_ok) {
        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "Last import: OK");
        ImGui::Text("Outputs: %zu", s._gltf_last_count);
        if (s._gltf_last_mesh[0] != '\0') {
            ImGui::TextWrapped("Mesh: %s", s._gltf_last_mesh);
        }
    } else if (s._gltf_last_error[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "Last import: FAILED");
        ImGui::TextWrapped("%s", s._gltf_last_error);
    }
}

// ---- Material Override tab -------------------------------------------------

inline AssetId tex_asset_id(const char *path) noexcept {
    if (!path || path[0] == '\0') {
        return {};
    }
    return AssetId::from_logical_path(StringView(path));
}

inline MaterialShadingModel to_shading_model(S32 v) noexcept {
    if (v == static_cast<S32>(MaterialShadingModel::Unlit)) {
        return MaterialShadingModel::Unlit;
    }
    if (v == static_cast<S32>(MaterialShadingModel::Standard)) {
        return MaterialShadingModel::Standard;
    }
    return MaterialShadingModel::PBR;
}

inline MaterialBlendMode to_blend_mode(S32 v) noexcept {
    if (v == static_cast<S32>(MaterialBlendMode::Masked)) {
        return MaterialBlendMode::Masked;
    }
    if (v == static_cast<S32>(MaterialBlendMode::Transparent)) {
        return MaterialBlendMode::Transparent;
    }
    return MaterialBlendMode::Opaque;
}

inline void draw_material_tab(AssetPanelCtx &ctx, World &world, DevToolsState &tools,
                              AssetPanelState &s) noexcept {
    Thing selected = tools.selected;
    if (selected.is_nil() || world.is_dead(selected)) {
        ImGui::TextDisabled("No selected thing.");
        return;
    }

    if (!world.try_get<MeshRendererPart>(selected)) {
        ImGui::TextDisabled("Selected thing has no MeshRendererPart.");
        return;
    }

    ImGui::Text("Thing #%u", selected.idx());

    if (MaterialOverridePart *op = world.try_get<MaterialOverridePart>(selected)) {
        if (op->material_path.size() != 0) {
            ImGui::TextWrapped("Current override: %s", op->material_path.c_str());
        }
    } else {
        ImGui::TextDisabled("No material override.");
    }

    ImGui::Separator();

    ImGui::InputText("Output .fmat##mat_out", s.mat_output, sizeof(s.mat_output));
    ImGui::ColorEdit4("Base Color##mat_color", s.mat_base_color);

    const char *shading_items[] = {"Unlit", "Standard", "PBR"};
    ImGui::Combo("Shading Model##mat_shading", &s.mat_shading_model, shading_items, 3);

    const char *blend_items[] = {"Opaque", "Masked", "Transparent"};
    ImGui::Combo("Blend Mode##mat_blend", &s.mat_blend_mode, blend_items, 3);

    ImGui::DragFloat("Metallic##mat_metallic", &s.mat_metallic, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Roughness##mat_roughness", &s.mat_roughness, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Alpha##mat_alpha", &s.mat_alpha, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Alpha Cutoff##mat_cutoff", &s.mat_alpha_cutoff, 0.01f, 0.0f, 1.0f);

    ImGui::Separator();

    ImGui::InputText("Albedo .ftex##mat_albedo", s.mat_albedo, sizeof(s.mat_albedo));
    ImGui::InputText("Normal .ftex##mat_normal", s.mat_normal, sizeof(s.mat_normal));
    ImGui::InputText("Extra .ftex##mat_extra", s.mat_extra, sizeof(s.mat_extra));
    ImGui::Checkbox("Force Cook##mat_force", &s.mat_force);

    ImGui::Separator();

    if (ImGui::Button("Cook / Apply##mat_apply")) {
        if (!ctx.is_valid()) {
            FR_LOG_ERR("[AssetPanel] Invalid AssetPanelCtx for material cook.");
        } else if (s.mat_output[0] == '\0') {
            FR_LOG_ERR("[AssetPanel] Material output path is empty.");
        } else {
            asscooker::MaterialCookDesc desc{};
            desc.albedo_texture = tex_asset_id(s.mat_albedo);
            desc.normal_texture = tex_asset_id(s.mat_normal);
            desc.extra_texture = tex_asset_id(s.mat_extra);
            desc.base_color_factor[0] = s.mat_base_color[0];
            desc.base_color_factor[1] = s.mat_base_color[1];
            desc.base_color_factor[2] = s.mat_base_color[2];
            desc.base_color_factor[3] = s.mat_base_color[3];
            desc.metallic_factor = s.mat_metallic;
            desc.roughness_factor = s.mat_roughness;
            desc.alpha = s.mat_alpha;
            desc.alpha_cutoff = s.mat_alpha_cutoff;
            desc.shading_model = to_shading_model(s.mat_shading_model);
            desc.blend_mode = to_blend_mode(s.mat_blend_mode);

            DynamicArray<asscooker::CookedAssetOutput> outputs(ctx.alloc);
            asscooker::CookOptions opts{};
            opts.force = s.mat_force;
            opts.output_id = AssetId::from_logical_path(StringView(s.mat_output));

            if (asscooker::cook_material_ex(desc, StringView(s.mat_output), &outputs, opts)) {
                ctx.catalog->add_or_replace(outputs.slice(), StringView("material override"));

                for (const asscooker::CookedAssetOutput &out : outputs.slice()) {
                    if (out.id.is_valid() && out.kind != AssetKind::Unknown &&
                        out.path.size() != 0) {
                        ctx.registry->register_loose_asset(out.id, out.kind, out.path.view(),
                                                           out.content_hash);
                    }
                }

                if (!StringView(s.manifest_path).is_empty()) {
                    if (!ctx.catalog->build_loose_manifest(StringView(s.manifest_path))) {
                        FR_LOG_ERR("[AssetPanel] Failed to rebuild manifest: {}",
                                   StringView(s.manifest_path));
                    }
                }

                MaterialOverridePart *op = world.try_get<MaterialOverridePart>(selected);
                if (!op) {
                    op = &world.emplace_now<MaterialOverridePart>(selected);
                } else if (op->material_handle.is_valid()) {
                    ctx.assets->unload_material(op->material_handle);
                }

                op->material_path = String::from_view(StringView(s.mat_output));
                op->material_id = AssetId::from_logical_path(op->material_path.view());
                op->resolved_material_id = {};
                op->material_handle = {};

                resolve_render_assets(world, *ctx.assets);
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Remove Override##mat_remove")) {
        if (MaterialOverridePart *op = world.try_get<MaterialOverridePart>(selected)) {
            if (op->material_handle.is_valid()) {
                ctx.assets->unload_material(op->material_handle);
            }
            world.destroy_now<MaterialOverridePart>(selected);
        }
    }
}

// ---- Environment HDR tab ---------------------------------------------------

inline void draw_environment_tab(AssetPanelCtx &ctx, World &world, AssetPanelState &s) noexcept {
    ImGui::InputText("Source HDR##env_src", s.env_source, sizeof(s.env_source));
    ImGui::InputText("Output .ftex##env_out", s.env_output, sizeof(s.env_output));
    ImGui::Checkbox("Force Recook##env_force", &s.env_force);

    ImGui::Separator();

    // Current scene environment info
    EnvironmentState *env = world.try_get_resource<EnvironmentState>();

    if (env) {
        ImGui::Checkbox("Enabled##env_enabled", &env->enabled);
        if (env->texture_path.size() != 0) {
            ImGui::TextWrapped("Scene env: %s", env->texture_path.c_str());
        } else {
            ImGui::TextDisabled("No environment texture loaded.");
        }
        ImGui::Text("Handle: %s", env->texture_handle.is_valid() ? "valid" : "invalid");
    } else {
        ImGui::TextDisabled("No EnvironmentState resource registered.");
    }

    ImGui::Separator();

    if (ImGui::Button("Import / Apply HDR##env_import")) {
        if (!ctx.is_valid()) {
            FR_LOG_ERR("[AssetPanel] Invalid AssetPanelCtx for HDR import.");
        } else if (s.env_source[0] == '\0' || s.env_output[0] == '\0') {
            FR_LOG_ERR("[AssetPanel] HDR source or output path is empty.");
        } else {
            String src = String::from_view(ctx.alloc, StringView(s.env_source));

            if (!file::exists(src)) {
                FR_LOG_ERR("[AssetPanel] HDR source file not found: {}", src.view());
            } else {
                DynamicArray<asscooker::CookedAssetOutput> outputs(ctx.alloc);
                asscooker::CookOptions opts{};
                opts.force = s.env_force;
                opts.output_id = AssetId::from_logical_path(StringView(s.env_output));

                if (asscooker::cook_texture_ex(StringView(s.env_source), StringView(s.env_output),
                                               false, &outputs, opts)) {
                    for (const asscooker::CookedAssetOutput &out : outputs.slice()) {
                        if (out.id.is_valid() && out.kind != AssetKind::Unknown &&
                            out.path.size() != 0) {
                            ctx.registry->register_loose_asset(out.id, out.kind, out.path.view(),
                                                               out.content_hash);
                        }
                    }

                    ctx.catalog->add_or_replace(outputs.slice(), StringView(s.env_source));
                    if (!ctx.catalog->build_loose_manifest(StringView(s.manifest_path))) {
                        FR_LOG_ERR("[AssetPanel] Failed to rebuild manifest: {}",
                                   StringView(s.manifest_path));
                    }

                    if (!env) {
                        env = &world.emplace_resource<EnvironmentState>();
                    }

                    if (env->texture_handle.is_valid()) {
                        ctx.assets->unload_texture(env->texture_handle);
                    }

                    env->texture_path = String::from_view(StringView(s.env_output));
                    env->texture_id = AssetId::from_logical_path(env->texture_path.view());
                    env->resolved_texture_id = {};
                    env->texture_handle = {};
                    env->enabled = true;

                    resolve_environment(world, *ctx.assets);
                    FR_LOG_OK("[AssetPanel] Imported HDR environment: {}",
                              StringView(s.env_output));
                } else {
                    FR_LOG_ERR("[AssetPanel] Failed to cook HDR: {}", StringView(s.env_source));
                }
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Resolve##env_resolve") && ctx.assets) {
        resolve_environment(world, *ctx.assets);
    }
}

} // namespace impl

// =============================================================================
// draw_asset_panel — public entry point
// =============================================================================

/// @brief Draws the asset operations panel as a tabbed window.
/// Call between ImGui::Begin / ImGui::End.
inline void draw_asset_panel(AssetPanelCtx &ctx, World &world, DevToolsState &tools,
                             AssetPanelState &state) noexcept {
    if (!ImGui::BeginTabBar("##asset_panel_tabs")) {
        return;
    }

    if (ImGui::BeginTabItem("glTF Import##gltf_tab")) {
        ImGui::PushID("gltf");
        impl::draw_gltf_tab(ctx, world, tools, state);
        ImGui::PopID();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Material Override##mat_tab")) {
        ImGui::PushID("material");
        impl::draw_material_tab(ctx, world, tools, state);
        ImGui::PopID();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Environment HDR##env_tab")) {
        ImGui::PushID("environment");
        impl::draw_environment_tab(ctx, world, state);
        ImGui::PopID();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

} // namespace fr::devtools

FR_TYPE(fr::devtools::AssetPanelCtx);
