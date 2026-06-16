/**
 * @file editor_commands.hpp
 * @author Kiju
 *
 * @brief Safe world mutation helpers used by runtime devtools.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/core/meta.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/state.hpp"
#include "fr/scene/render_assets.hpp"

namespace fr::devtools {

/// @brief Selects a thing and updates inspector navigation.
inline void select_thing(DevToolsState &tools, Thing thing) noexcept {
    set_selected_thing(tools, thing);
}

/// @brief Clears the current selection.
inline void clear_selection(DevToolsState &tools) noexcept {
    set_selected_thing(tools, Thing::nil());
}

/// @brief Kills a thing, releases its render assets, and clears selection if needed.
inline void kill_thing(World &world, DevToolsState &tools, Thing thing,
                       AssetManager *assets = nullptr) noexcept {
    if (thing.is_nil() || world.is_dead(thing)) {
        return;
    }

    if (assets) {
        release_thing_render_assets(world, *assets, thing);
    }

    if (tools.selected == thing) {
        set_selected_thing(tools, Thing::nil());
    }

    if (tools.hovered == thing) {
        tools.hovered = Thing::nil();
    }

    world.kill(thing);
}

/// @brief Destroys a specific part from a thing, releasing render assets if needed.
inline void destroy_part(World &world, Thing thing, TypeIdx part_type,
                         AssetManager *assets = nullptr) noexcept {
    if (thing.is_nil() || world.is_dead(thing) || part_type.is_nil()) {
        return;
    }

    if (!world.has_raw(part_type, thing)) {
        return;
    }

    if (assets) {
        release_part_render_assets(world, *assets, thing, part_type);
    }

    world.destroy_now_raw(part_type, thing);
}

/// @brief Inserts a default-constructed part onto a thing.
inline void insert_default_part(World &world, Thing thing, TypeIdx part_type) noexcept {
    if (thing.is_nil() || world.is_dead(thing) || part_type.is_nil()) {
        return;
    }

    if (world.has_raw(part_type, thing)) {
        return;
    }

    world.insert_default_raw(part_type, thing);
}

} // namespace fr::devtools
