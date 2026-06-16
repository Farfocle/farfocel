/**
 * @file scene_io.hpp
 * @brief Redirect shim — scene IO helpers moved to fr/scene/scene_io.hpp.
 */

#pragma once

#include "fr/scene/scene_io.hpp"

namespace fr::devtools {
/// @brief Backward-compat alias. Use fr::scene::load_scene() instead.
inline bool load_scene_replacing_world(World &world, AssetManager &assets,
                                       StringView input_path) noexcept {
    return fr::scene::load_scene(world, assets, input_path);
}

} // namespace fr::devtools
