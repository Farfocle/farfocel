/**
 * @file thing_explorer.cpp
 * @author Kiju
 *
 * @brief Left-panel thing list for the world inspector.
 */

#include "fr/devtools/thing_explorer.hpp"

#include <cstdio>
#include <imgui.h>

namespace fr::devtools {

void thing_explorer_ui(World &world, InspectorState &state) noexcept {
    if (!ImGui::CollapsingHeader("Things", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const USize count = world.alive_thing_count();
    char header[64];
    std::snprintf(header, sizeof(header), "%zu alive", count);

    ImGui::TextDisabled("%s", header);
    ImGui::SameLine();

    if (ImGui::SmallButton("Spawn")) {
        state.selected_thing = world.spawn();
        state.active_view = InspectorState::View::Thing;
    }

    ImGui::Spacing();

    world.for_each_alive_thing([&](Thing t) {
        char label[64];
        std::snprintf(label, sizeof(label), "Thing #%u  gen:%u", t.idx(), t.gen());

        ImGui::PushID(static_cast<int>(t.as_raw()));

        const bool selected =
            (state.active_view == InspectorState::View::Thing && t == state.selected_thing);
        const float kill_w = ImGui::CalcTextSize("Kill").x +
                             ImGui::GetStyle().FramePadding.x * 2.0f +
                             ImGui::GetStyle().ItemSpacing.x;

        if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_None,
                              ImVec2(ImGui::GetContentRegionAvail().x - kill_w, 0.0f))) {
            state.selected_thing = t;
            state.active_view = InspectorState::View::Thing;
        }

        ImGui::SameLine(ImGui::GetContentRegionMax().x -
                        (ImGui::CalcTextSize("Kill").x + ImGui::GetStyle().FramePadding.x * 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.25f, 0.25f, 1.0f));

        if (ImGui::SmallButton("Kill")) {
            if (state.selected_thing == t) {
                state.selected_thing = Thing::nil();

                if (state.active_view == InspectorState::View::Thing) {
                    state.active_view = InspectorState::View::None;
                }
            }

            world.kill(t);
        }

        ImGui::PopStyleColor(2);
        ImGui::PopID();
    });
}

} // namespace fr::devtools
