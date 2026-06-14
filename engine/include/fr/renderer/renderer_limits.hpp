/**
 * @file renderer_limits.hpp
 * @author Tfoedy
 * @brief Compile-time renderer capacity limits.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_lights.hpp"

namespace fr {

/// @brief Maximum transform records uploaded for one frame.
constexpr USize MAX_INSTANCES = 65536;

/// @brief Maximum material records uploaded for one frame.
constexpr USize MAX_RENDER_MATERIALS = MAX_INSTANCES;

/// @brief Maximum point lights uploaded for one frame.
constexpr USize MAX_POINT_LIGHTS = 1024;

/// @brief Maximum directional lights uploaded for one frame.
constexpr USize MAX_DIR_LIGHTS = 4;

/// @brief Maximum spot lights uploaded for one frame.
constexpr USize MAX_RENDER_SPOT_LIGHTS = MAX_SPOT_LIGHTS;

/// @brief Maximum point shadow maps used by the renderer.
constexpr USize MAX_POINT_SHADOWS = MAX_SHADOWED_POINT_LIGHTS;

/// @brief Maximum spot shadow tiles used by the renderer.
constexpr USize MAX_SPOT_SHADOWS = MAX_SHADOWED_SPOT_LIGHTS;

} // namespace fr
