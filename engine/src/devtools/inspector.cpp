/**
 * @file inspector.cpp
 * @author Kiju
 *
 * @brief World inspector devtools panel implementation.
 */

#include "fr/devtools/inspector.hpp"

#include <cstdio>
#include <imgui.h>

#include "fr/core/meta.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"
#include "fr/devtools/imgui_archive.hpp"

namespace fr::devtools {

// ========================================================== thing_parts_editor

void thing_parts_editor(World &world, Thing thing) noexcept {
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

// ============================================================= Internal panels

namespace impl {

static void thing_explorer(World &world, InspectorState &state) noexcept {
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

static void pool_explorer(World &world, InspectorState &state) noexcept {
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
        snprintf(text, sizeof(text), "%-24s  %zu", pm.name, sz);

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

static void thing_inspector(World &world, InspectorState &state) noexcept {
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

    thing_parts_editor(world, t);
}

static void pool_inspector(World &world, InspectorState &state) noexcept {
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

} // namespace impl

// =================================================================== Inspector

void inspector(World &world, InspectorState &state) noexcept {
    const F32 left_w = 260.0f;

    ImGui::BeginChild("##insp_left", ImVec2(left_w, 0.0f), ImGuiChildFlags_Borders);
    impl::thing_explorer(world, state);
    ImGui::Spacing();
    impl::pool_explorer(world, state);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##insp_right", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    switch (state.active_view) {
    case InspectorState::View::Thing:
        impl::thing_inspector(world, state);
        break;
    case InspectorState::View::Pool:
        impl::pool_inspector(world, state);
        break;
    default:
        ImGui::TextDisabled("Select a thing or a part pool.");
        break;
    }

    ImGui::EndChild();
}

} // namespace fr::devtools
