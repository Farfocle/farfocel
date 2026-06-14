/**
 * @file thing_inspector.cpp
 * @author Kiju
 *
 * @brief Right-panel thing detail view for the world inspector.
 */

#include "fr/devtools/thing_inspector.hpp"

#include <imgui.h>

#include "fr/devtools/inspector.hpp"

namespace fr::devtools {

void thing_inspector_ui(World &world, InspectorState &state) noexcept {
    Thing t = state.selected_thing;

    if (t.is_nil()) {
        ImGui::TextDisabled("Select a thing in the left panel.");
        return;
    }

    if (world.is_dead(t)) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Thing is dead.");
        state.selected_thing = Thing::nil();
        state.active_view = InspectorState::View::None;

        return;
    }

    ImGui::Text("Thing  #%u  gen:%u", t.idx(), t.gen());

    const float kill_w = ImGui::CalcTextSize("Kill").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - kill_w);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.25f, 0.25f, 1.0f));

    if (ImGui::Button("Kill")) {
        world.kill(t);
        state.selected_thing = Thing::nil();
        state.active_view = InspectorState::View::None;
        ImGui::PopStyleColor(2);
        return;
    }

    ImGui::PopStyleColor(2);
    ImGui::Separator();

    thing_parts_editor_ui(world, t);
}

} // namespace fr::devtools
