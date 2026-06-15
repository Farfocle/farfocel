/**
 * @file gltf_import_panel.hpp
 * @author Tfoedy
 * @brief ImGui panel for importing glTF source assets through the development asset pipeline.
 */

#pragma once

#include <cstring>

#include <imgui.h>

#include "fr/asscooker/dev_asset_importer.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/devtools/world_actions.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker::imgui {

/**
 * @brief Persistent UI state for the glTF import panel.
 */
struct GltfImportPanelState {
    char source_path[512]{};
    char import_name[128]{};

    char cooked_root[256]{"assets"};
    char manifest_path[256]{"assets/dev.fmanifest"};

    char last_mesh_path[512]{};
    char last_error[256]{};

    USize last_output_count{0};

    bool force{true};
    bool spawn_after_import{true};

    bool last_import_ok{false};
    bool import_requested_this_frame{false};
};

/**
 * @brief Copies a StringView into a null-terminated fixed-size buffer.
 */
inline void copy_to_buffer(char *dst, USize dst_size, StringView src) noexcept {
    if (!dst || dst_size == 0) {
        return;
    }

    const USize copy_size = src.size() < dst_size - 1 ? src.size() : dst_size - 1;

    if (copy_size > 0) {
        std::memcpy(dst, src.data(), copy_size);
    }

    dst[copy_size] = '\0';
}

/**
 * @brief Clears the last import status.
 */
inline void clear_gltf_import_status(GltfImportPanelState &state) noexcept {
    state.last_mesh_path[0] = '\0';
    state.last_error[0] = '\0';
    state.last_output_count = 0;
    state.last_import_ok = false;
    state.import_requested_this_frame = false;
}

/**
 * @brief Builds a development import context from panel state.
 */
inline DevAssetImportContext make_import_context(DevAssetImportContext base,
                                                 GltfImportPanelState &state) noexcept {
    base.cooked_root = StringView(state.cooked_root);
    base.manifest_path = StringView(state.manifest_path);
    return base;
}

/**
 * @brief Draws source import settings.
 */
inline void draw_gltf_import_settings(GltfImportPanelState &state) noexcept {
    ImGui::InputText("Source glTF/GLB", state.source_path, sizeof(state.source_path));

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Path to source .gltf or .glb file.");
    }

    ImGui::InputText("Import Name", state.import_name, sizeof(state.import_name));

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional stable asset name. Empty uses source file stem.");
    }

    ImGui::InputText("Cooked Root", state.cooked_root, sizeof(state.cooked_root));

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Base directory for cooked runtime assets.");
    }

    ImGui::InputText("Manifest Path", state.manifest_path, sizeof(state.manifest_path));

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Development .fmanifest rebuilt after successful import.");
    }

    ImGui::Checkbox("Force Recook", &state.force);
    ImGui::Checkbox("Spawn After Import", &state.spawn_after_import);
}

/**
 * @brief Draws last import result.
 */
inline void draw_gltf_import_status(const GltfImportPanelState &state) noexcept {
    if (state.last_import_ok) {
        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "Last import: OK");
        ImGui::Text("Outputs: %zu", state.last_output_count);

        if (state.last_mesh_path[0] != '\0') {
            ImGui::TextWrapped("Mesh: %s", state.last_mesh_path);
        }

        return;
    }

    if (state.last_error[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "Last import: FAILED");
        ImGui::TextWrapped("%s", state.last_error);
    }
}

/**
 * @brief Imports glTF using the development asset pipeline.
 *
 * @details
 * Returns the importer result. When no import was requested this frame, result.ok is false.
 */
[[nodiscard]] inline ImportedModelResult
draw_gltf_import_panel(DevAssetImportContext base_import_ctx,
                       fr::devtools::EditorContext &editor_ctx,
                       GltfImportPanelState &state) noexcept {
    FR_ASSERT(editor_ctx.is_valid(), "EditorContext must be valid");

    state.import_requested_this_frame = false;

    if (ImGui::CollapsingHeader("glTF Import", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_gltf_import_settings(state);

        ImGui::Separator();

        if (ImGui::Button("Import glTF")) {
            state.import_requested_this_frame = true;
            state.last_import_ok = false;
            state.last_mesh_path[0] = '\0';
            state.last_error[0] = '\0';
            state.last_output_count = 0;

            if (state.source_path[0] == '\0') {
                copy_to_buffer(state.last_error, sizeof(state.last_error),
                               StringView("Source path is empty."));
                FR_LOG_ERR("[Cooker] Cannot import glTF from an empty source path.");
                draw_gltf_import_status(state);
                return {};
            }

            DevAssetImportContext import_ctx = make_import_context(base_import_ctx, state);

            if (!import_ctx.is_valid()) {
                copy_to_buffer(state.last_error, sizeof(state.last_error),
                               StringView("Invalid DevAssetImportContext."));
                FR_LOG_ERR("[Cooker] Invalid DevAssetImportContext in glTF import panel.");
                draw_gltf_import_status(state);
                return {};
            }

            const StringView source_path(state.source_path);
            const StringView import_name =
                state.import_name[0] != '\0' ? StringView(state.import_name) : StringView{};

            ImportedModelResult result =
                import_gltf_model(import_ctx, source_path, import_name, state.force);

            if (!result.ok) {
                copy_to_buffer(state.last_error, sizeof(state.last_error),
                               StringView("import_gltf_model() failed. See log for details."));
                draw_gltf_import_status(state);
                return result;
            }

            state.last_import_ok = true;
            state.last_output_count = result.output_count;
            copy_to_buffer(state.last_mesh_path, sizeof(state.last_mesh_path),
                           result.mesh_path.view());

            if (state.spawn_after_import) {
                fr::devtools::spawn_mesh(editor_ctx, result.mesh_path.view());
            }

            draw_gltf_import_status(state);
            return result;
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear Status")) {
            clear_gltf_import_status(state);
        }

        ImGui::Separator();
        draw_gltf_import_status(state);
    }

    return {};
}

} // namespace fr::asscooker::imgui
