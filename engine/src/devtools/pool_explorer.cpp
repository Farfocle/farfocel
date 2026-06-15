/**
 * @file pool_explorer.cpp
 * @author Kiju
 *
 * @brief Left-panel part-pool list for the world inspector.
 */

#include "fr/devtools/pool_explorer.hpp"

#include <cstdio>
#include <imgui.h>

#include "fr/core/meta.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"

namespace fr::devtools {

void pool_explorer_ui(World &world, InspectorState &state) noexcept {
    if (!ImGui::CollapsingHeader("Part Pools", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const PartMetaRegistry &meta = world.part_meta();

    bool any = false;
    for (USize i = 0; i < MAX_PARTS; ++i) {
        TypeIdx tidx = TypeIdx::from_idx(static_cast<TypeIdx::IDX>(i));
        if (!meta.has(tidx)) {
            continue;
        }

        any = true;
        const PartMeta &pm = meta.get(tidx);
        const USize sz = world.pool_size_raw(tidx);

        ImGui::PushID(static_cast<int>(i));

        char text[80];
        std::snprintf(text, sizeof(text), "%-24s  %zu", pm.name, sz);

        const bool selected =
            (state.active_view == InspectorState::View::Pool && tidx == state.selected_pool);
        if (ImGui::Selectable(text, selected)) {
            state.selected_pool = tidx;
            state.active_view = InspectorState::View::Pool;
        }

        ImGui::PopID();
    }

    if (!any) {
        ImGui::TextDisabled("No part pools registered yet.");
    }
}

} // namespace fr::devtools
