/**
 * @file renderer_constants.hpp
 * @author Tfoedy
 * @brief Renderer compile-time capacities.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_light_data.hpp"

namespace fr {

constexpr USize MAX_INSTANCES = 65536;
constexpr USize MAX_POINT_LIGHTS = 1024;
constexpr USize MAX_DIR_LIGHTS = 4;

constexpr USize MAX_RENDER_SPOT_LIGHTS = MAX_SPOT_LIGHTS;
constexpr USize MAX_POINT_SHADOWS = MAX_SHADOWED_POINT_LIGHTS;
constexpr USize MAX_SPOT_SHADOWS = MAX_SHADOWED_SPOT_LIGHTS;

} // namespace fr
