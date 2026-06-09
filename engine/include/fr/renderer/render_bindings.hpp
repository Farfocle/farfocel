/**
 * @file render_bindings.hpp
 * @author Tfoedy
 * @brief Shared binding constants used by C++ renderer code and GLSL shaders.
 */

#pragma once

#include "fr/core/typedefs.hpp"

namespace fr::bindings {

// present pass
constexpr U32 PRESENT_COLOR = 0;

// SSBOs
constexpr U32 SSBO_TRANSFORMS = 0;
constexpr U32 SSBO_CAMERA = 1;
constexpr U32 SSBO_POINT_LIGHTS = 2;
constexpr U32 SSBO_DIR_LIGHTS = 3;
constexpr U32 SSBO_POINT_SHADOWS = 4;
constexpr U32 SSBO_SPOT_LIGHTS = 5;
constexpr U32 SSBO_SPOT_SHADOWS = 6;

// Geometry pass
constexpr U32 TEX_ALBEDO = 0;
constexpr U32 TEX_NORMAL = 1;
constexpr U32 TEX_EXTRA = 2;

// HBAO pass
constexpr U32 HBAO_NORMAL = 0;
constexpr U32 HBAO_DEPTH = 1;

// IBL generation passes
constexpr U32 IBL_SOURCE_MAP = 0;
constexpr U32 IBL_SOURCE_EQUIRECT = IBL_SOURCE_MAP;

// Lighting pass
constexpr U32 GBUFFER_ALBEDO = 0;
constexpr U32 GBUFFER_NORMAL = 1;
constexpr U32 GBUFFER_EXTRA = 2;
constexpr U32 GBUFFER_DEPTH = 3;
constexpr U32 SHADOW_MAP = 4;
constexpr U32 IBL_ENVIRONMENT = 5;
constexpr U32 HBAO_MAP = 6;
constexpr U32 POINT_SHADOW_MAP_BASE = 7;
constexpr U32 SPOT_SHADOW_MAP = 11;
constexpr U32 IBL_IRRADIANCE = 12;
constexpr U32 IBL_PREFILTERED = 13;
constexpr U32 IBL_BRDF_LUT = 14;

} // namespace fr::bindings
