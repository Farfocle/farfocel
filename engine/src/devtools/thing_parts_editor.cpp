/**
 * @file thing_parts_editor.cpp
 * @author Kiju
 *
 * @brief Inline parts editor for a single thing.
 */

#include "fr/devtools/inspector.hpp"

#include <imgui.h>

#include "fr/core/meta.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"
#include "fr/devtools/imgui_archive.hpp"

namespace fr::devtools {

void thing_parts_editor_ui(World &world, Thing thing) noexcept {
    if (thing.is_nil() || world.is_dead(thing)) {
        return;
    }

    ImGuiWriterArchive archive{};
    const PartMetaRegistry &meta = world.part_meta();

    if (ImGui::CollapsingHeader("Parts", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool has_any = false;

        for (USize i = 0; i < MAX_PARTS; ++i) {
            TypeIdx tidx = TypeIdx::from_idx(static_cast<TypeIdx::IDX>(i));
            if (!meta.has(tidx) || !world.has_raw(tidx, thing)) {
                continue;
            }

            has_any = true;
            const PartMeta &pm = meta.get(tidx);

            ImGui::PushID(static_cast<int>(i));

            const F32 destroy_w =
                ImGui::CalcTextSize("Destroy").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            const bool open = ImGui::TreeNode(pm.name);
            ImGui::SameLine(ImGui::GetContentRegionMax().x - destroy_w);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));

            const bool destroy_clicked = ImGui::SmallButton("Destroy");
            ImGui::PopStyleColor(2);

            if (open) {
                if (pm.imgui_part) {
                    void *ptr = world.try_get_raw(tidx, thing);

                    if (ptr) {
                        pm.imgui_part(ptr, archive);
                    }
                } else {
                    ImGui::TextDisabled("(no view)");
                }
                ImGui::TreePop();
            }

            if (destroy_clicked) {
                world.destroy_now_raw(tidx, thing);
            }

            ImGui::PopID();
        }

        if (!has_any) {
            ImGui::TextDisabled("No parts.");
        }
    }

    if (ImGui::CollapsingHeader("Insert Part")) {
        bool has_insertable = false;

        for (USize i = 0; i < MAX_PARTS; ++i) {
            TypeIdx tidx = TypeIdx::from_idx(static_cast<TypeIdx::IDX>(i));
            if (!meta.has(tidx) || world.has_raw(tidx, thing)) {
                continue;
            }

            const PartMeta &pm = meta.get(tidx);
            if (!pm.insert_default) {
                continue;
            }

            has_insertable = true;
            ImGui::PushID(static_cast<int>(i) + 10000);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(pm.name);

            const float insert_w =
                ImGui::CalcTextSize("Insert").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - insert_w);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.60f, 0.22f, 1.0f));

            if (ImGui::SmallButton("Insert")) {
                world.insert_default_raw(tidx, thing);
            }

            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }

        if (!has_insertable) {
            ImGui::TextDisabled("All registered parts already present.");
        }
    }
}

} // namespace fr::devtools
