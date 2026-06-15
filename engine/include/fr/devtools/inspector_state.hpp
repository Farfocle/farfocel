/**
 * @file inspector_state.hpp
 * @author Tfoedy
 * @brief Persistent state for world inspector devtools panel.
 */

#pragma once

#include "fr/core/meta.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr::devtools {

/**
 * @brief Persistent UI state for the world inspector.
 */
struct InspectorState {
    /**
     * @brief Right-panel view mode.
     */
    enum class View : U8 { None, Thing, Pool };

    /**
     * @brief Currently selected thing.
     */
    Thing selected_thing{Thing::nil()};

    /**
     * @brief Currently selected part pool.
     */
    TypeIdx selected_pool{TypeIdx::nil()};

    /**
     * @brief Active right-panel view.
     */
    View active_view{View::None};
};

} // namespace fr::devtools
