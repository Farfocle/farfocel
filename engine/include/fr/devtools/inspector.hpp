/**
 * @file inspector.hpp
 * @author Kiju
 *
 * @brief World inspector devtools panel.
 */

#pragma once

#include "fr/core/meta.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"

namespace fr::devtools {

/// @brief Persistent UI state for the world inspector.
struct InspectorState {
    /// Which right-panel view is currently shown.
    enum class View : U8 { None, Thing, Pool };

    /// Currently selected thing (nil = nothing selected).
    Thing selected_thing{Thing::nil()};

    /// Currently selected part pool (nil = nothing selected).
    TypeIdx selected_pool{TypeIdx::nil()};

    /// Which right-panel view is active.
    View active_view{View::None};
};

/// @brief Draws the parts editor for a single thing inline.
void thing_parts_editor(World &world, Thing thing) noexcept;

/**
 * @brief Draws the world inspector inside the current ImGui window.
 *
 * @param world  The world to inspect and mutate.
 * @param state  Persistent UI state. Keep alive between frames.
 *
 * @note Must be called between `ImGui::Begin` / `ImGui::End`.
 */
void inspector(World &world, InspectorState &state) noexcept;

} // namespace fr::devtools
