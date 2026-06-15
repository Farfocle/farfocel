/**
 * @file editor_commands.hpp
 * @author Tfoedy
 * @brief Safe world mutation helpers used by runtime devtools.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/devtools_state.hpp"
#include "fr/scene/render_asset_system.hpp"

namespace fr::devtools {

/**
 * @brief Context required by editor/devtools mutation commands.
 */
struct EditorContext {
    World *world{nullptr};
    AssetManager *assets{nullptr};
    DevToolsState *tools{nullptr};

    [[nodiscard]] bool is_valid() const noexcept {
        return world != nullptr && assets != nullptr && tools != nullptr;
    }
};

/**
 * @brief Selects a thing in all devtools panels.
 */
inline void select_thing(EditorContext &ctx, Thing thing) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    if (thing.is_nil() || ctx.world->is_dead(thing)) {
        set_selected_thing(*ctx.tools, Thing::nil());
        return;
    }

    set_selected_thing(*ctx.tools, thing);
}

/**
 * @brief Clears current devtools selection.
 */
inline void clear_selection(EditorContext &ctx) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");
    set_selected_thing(*ctx.tools, Thing::nil());
}

/**
 * @brief Kills a thing through the editor command layer.
 *
 * @details
 * Runtime asset handles owned by render parts are released before the entity is killed.
 */
inline void kill_thing(EditorContext &ctx, Thing thing) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    if (thing.is_nil() || ctx.world->is_dead(thing)) {
        return;
    }

    RenderAssetSystem::release_thing(*ctx.world, *ctx.assets, thing);

    if (ctx.tools->selected == thing || ctx.tools->inspector.selected_thing == thing) {
        set_selected_thing(*ctx.tools, Thing::nil());
    }

    if (ctx.tools->hovered == thing) {
        ctx.tools->hovered = Thing::nil();
    }

    ctx.world->kill(thing);
}

/**
 * @brief Destroys a part through the editor command layer.
 *
 * @details
 * This releases runtime resources owned by known asset-backed parts before removing the part.
 */
inline void destroy_part(EditorContext &ctx, Thing thing, TypeIdx part_type) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    if (thing.is_nil() || ctx.world->is_dead(thing) || part_type.is_nil()) {
        return;
    }

    if (!ctx.world->has_raw(part_type, thing)) {
        return;
    }

    RenderAssetSystem::release_part(*ctx.world, *ctx.assets, thing, part_type);
    ctx.world->destroy_now_raw(part_type, thing);
}

/**
 * @brief Inserts a default-constructed part through the editor command layer.
 */
inline void insert_default_part(EditorContext &ctx, Thing thing, TypeIdx part_type) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    if (thing.is_nil() || ctx.world->is_dead(thing) || part_type.is_nil()) {
        return;
    }

    if (ctx.world->has_raw(part_type, thing)) {
        return;
    }

    ctx.world->insert_default_raw(part_type, thing);
}

} // namespace fr::devtools
