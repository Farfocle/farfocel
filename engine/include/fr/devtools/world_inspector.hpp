/**
 * @file world_inspector.hpp
 * @brief World inspector devtools panel (things, part pools, resources).
 */

#pragma once

#include <cstdio>
#include <imgui.h>

#include "fr/core/meta.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/devtools/imgui_archive.hpp"

namespace fr::devtools {

// ================================================================ Parts Editor

/// @brief Draws the parts editor for a single thing.
inline void draw_thing_parts_editor(World &world, Thing thing,
                                    AssetManager *assets = nullptr) noexcept {
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
                destroy_part(world, thing, tidx, assets);
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
                insert_default_part(world, thing, tidx);
            }

            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }

        if (!has_insertable) {
            ImGui::TextDisabled("All registered parts already present.");
        }
    }
}

// ============================================================= Internal Panels

namespace impl {

inline void draw_thing_explorer(World &world, DevToolsState &tools) noexcept {
    if (!ImGui::CollapsingHeader("Things", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const USize count = world.alive_thing_count();
    char header[64];
    std::snprintf(header, sizeof(header), "%zu alive", count);

    ImGui::TextDisabled("%s", header);
    ImGui::SameLine();

    if (ImGui::SmallButton("Spawn")) {
        Thing spawned = world.spawn();
        select_thing(tools, spawned);
    }

    ImGui::Spacing();

    world.each_alive_thing([&](Thing t) {
        char label[64];
        std::snprintf(label, sizeof(label), "Thing #%u  gen:%u", t.idx(), t.gen());

        ImGui::PushID(static_cast<int>(t.as_raw()));

        const bool selected =
            (tools.inspector_view == DevToolsState::InspectorView::Thing &&
             t == tools.selected);
        const float kill_w = ImGui::CalcTextSize("Kill").x +
                             ImGui::GetStyle().FramePadding.x * 2.0f +
                             ImGui::GetStyle().ItemSpacing.x;

        if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_None,
                              ImVec2(ImGui::GetContentRegionAvail().x - kill_w, 0.0f))) {
            select_thing(tools, t);
        }

        ImGui::SameLine(ImGui::GetContentRegionMax().x -
                        (ImGui::CalcTextSize("Kill").x +
                         ImGui::GetStyle().FramePadding.x * 2.0f));

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.25f, 0.25f, 1.0f));

        if (ImGui::SmallButton("Kill")) {
            kill_thing(world, tools, t);
        }

        ImGui::PopStyleColor(2);
        ImGui::PopID();
    });
}

inline void draw_pool_explorer(World &world, DevToolsState &tools) noexcept {
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

        const bool selected = (tools.inspector_view == DevToolsState::InspectorView::Pool &&
                               tidx == tools.inspector_pool);
        if (ImGui::Selectable(text, selected)) {
            tools.inspector_pool = tidx;
            tools.inspector_view = DevToolsState::InspectorView::Pool;
        }

        ImGui::PopID();
    }

    if (!any) {
        ImGui::TextDisabled("No part pools registered yet.");
    }
}

inline void draw_resource_explorer(World &world, DevToolsState &tools) noexcept {
    if (!ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    bool any = false;
    world.for_each_imgui_resource([&](TypeIdx tidx, void * /*ptr*/, const TypeMeta &meta) {
        any = true;

        ImGui::PushID(static_cast<int>(tidx.idx()));

        const bool selected =
            (tools.inspector_view == DevToolsState::InspectorView::Resource &&
             tidx == tools.inspector_resource);

        if (ImGui::Selectable(meta.name, selected)) {
            tools.inspector_resource = tidx;
            tools.inspector_view = DevToolsState::InspectorView::Resource;
        }

        ImGui::PopID();
    });

    if (!any) {
        ImGui::TextDisabled("No resources with ImGui views.");
    }
}

inline void draw_thing_inspector(World &world, DevToolsState &tools,
                                 AssetManager *assets = nullptr) noexcept {
    Thing t = tools.selected;

    if (t.is_nil()) {
        ImGui::TextDisabled("Select a thing in the left panel.");
        return;
    }

    if (world.is_dead(t)) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Thing is dead.");
        set_selected_thing(tools, Thing::nil());
        return;
    }

    ImGui::Text("Thing  #%u  gen:%u", t.idx(), t.gen());

    const float kill_w = ImGui::CalcTextSize("Kill").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - kill_w);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.25f, 0.25f, 1.0f));

    if (ImGui::Button("Kill")) {
        kill_thing(world, tools, t, assets);
        ImGui::PopStyleColor(2);
        return;
    }

    ImGui::PopStyleColor(2);
    ImGui::Separator();

    draw_thing_parts_editor(world, t, assets);
}

inline void draw_pool_inspector(World &world, DevToolsState &tools) noexcept {
    TypeIdx tidx = tools.inspector_pool;
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

    world.each_alive_thing([&](Thing t) {
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
            select_thing(tools, t);
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

inline void draw_resource_inspector(World &world, DevToolsState &tools) noexcept {
    TypeIdx tidx = tools.inspector_resource;

    void *ptr = world.try_get_resource_raw(tidx);

    if (!ptr) {
        ImGui::TextDisabled("Resource no longer present.");
        return;
    }

    const TypeMeta &meta = tidx.meta();
    ImGui::Text("Resource: %s", meta.name);
    ImGui::Separator();

    if (!meta.imgui_writer_shape) {
        ImGui::TextDisabled("(no view)");
        return;
    }

    ImGuiWriterArchive archive{};
    meta.imgui_writer_shape(archive, ptr);
}

} // namespace impl

// ============================================================= world_inspector

/// @brief Draws the world inspector panel. Must be called between ImGui::Begin / ImGui::End.
inline void draw_world_inspector(World &world, DevToolsState &tools,
                                 AssetManager *assets = nullptr) noexcept {
    const F32 left_w = 280.0f;

    ImGui::BeginChild("##insp_left", ImVec2(left_w, 0.0f), ImGuiChildFlags_Borders);
    impl::draw_thing_explorer(world, tools);
    ImGui::Spacing();
    impl::draw_pool_explorer(world, tools);
    ImGui::Spacing();
    impl::draw_resource_explorer(world, tools);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##insp_right", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

    switch (tools.inspector_view) {
    case DevToolsState::InspectorView::Thing:
        impl::draw_thing_inspector(world, tools, assets);
        break;

    case DevToolsState::InspectorView::Pool:
        impl::draw_pool_inspector(world, tools);
        break;

    case DevToolsState::InspectorView::Resource:
        impl::draw_resource_inspector(world, tools);
        break;

    default:
        ImGui::TextDisabled("Select a thing, part pool, or resource.");
        break;
    }

    ImGui::EndChild();
}

} // namespace fr::devtools
