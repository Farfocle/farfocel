/**
 * @file pool_inspector.cpp
 * @author Kiju
 *
 * @brief Right-panel part-pool detail view for the world inspector.
 */

#include "fr/devtools/pool_inspector.hpp"

#include <cstdio>
#include <imgui.h>

#include "fr/core/meta.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"
#include "fr/devtools/imgui_archive.hpp"

namespace fr::devtools {

void pool_inspector_ui(World &world, InspectorState &state) noexcept {
    TypeIdx tidx = state.selected_pool;
    const PartMetaRegistry &meta = world.part_meta();

    if (!meta.has(tidx)) {
        ImGui::TextDisabled("Pool no longer registered.");
        return;
    }

    const PartMeta &pm = meta.get(tidx);

    ImGui::Text("Pool: %s", pm.name);
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu things)", world.pool_size_raw(tidx));
    ImGui::Separator();

    if (world.pool_size_raw(tidx) == 0) {
        ImGui::TextDisabled("Pool is empty.");
        return;
    }

    ImGuiWriterArchive archive{};

    world.for_each_alive_thing([&](Thing t) {
        if (!world.has_raw(tidx, t)) {
            return;
        }

        char node_label[64];
        std::snprintf(node_label, sizeof(node_label), "Thing #%u  gen:%u", t.idx(), t.gen());

        ImGui::PushID(static_cast<int>(t.as_raw()));

        const float inspect_w =
            ImGui::CalcTextSize("Inspect").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const bool open = ImGui::TreeNode(node_label);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - inspect_w);

        if (ImGui::SmallButton("Inspect")) {
            state.selected_thing = t;
            state.active_view = InspectorState::View::Thing;
        }

        if (open) {
            if (pm.imgui_part) {
                void *part_ptr = world.try_get_raw(tidx, t);

                if (part_ptr) {
                    pm.imgui_part(part_ptr, archive);
                }
            } else {
                ImGui::TextDisabled("(no view)");
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    });
}

} // namespace fr::devtools
