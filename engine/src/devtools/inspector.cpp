/**
 * @file inspector.cpp
 * @author Kiju
 *
 * @brief World inspector — top-level compositor.
 */

#include "fr/devtools/inspector.hpp"

#include <imgui.h>

#include "fr/devtools/pool_explorer.hpp"
#include "fr/devtools/pool_inspector.hpp"
#include "fr/devtools/thing_explorer.hpp"
#include "fr/devtools/thing_inspector.hpp"

namespace fr::devtools {

void inspector_ui(World &world, InspectorState &state) noexcept {
    const F32 left_w = 260.0f;

    ImGui::BeginChild("##insp_left", ImVec2(left_w, 0.0f), ImGuiChildFlags_Borders);
    thing_explorer_ui(world, state);
    ImGui::Spacing();
    pool_explorer_ui(world, state);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##insp_right", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    switch (state.active_view) {
    case InspectorState::View::Thing:
        thing_inspector_ui(world, state);
        break;
    case InspectorState::View::Pool:
        pool_inspector_ui(world, state);
        break;
    default:
        ImGui::TextDisabled("Select a thing or a part pool.");
        break;
    }

    ImGui::EndChild();
}

} // namespace fr::devtools
