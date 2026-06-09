/**
 * @file renderer_resources.hpp
 * @author Tfoedy
 * @brief Renderer-owned GPU resource groups.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/renderer_constants.hpp"

namespace fr {

struct RendererGlobalBuffers {
    BufferHandle transform_ssbo{};
    BufferHandle shadow_transform_ssbo{};
    BufferHandle camera_ssbo{};

    BufferHandle point_lights_ssbo{};
    BufferHandle spot_lights_ssbo{};
    BufferHandle dir_lights_ssbo{};

    BufferHandle point_shadows_ssbo{};
    BufferHandle spot_shadows_ssbo{};
};

struct RendererFallbackTextures {
    TextureHandle white{};
    TextureHandle black{};
    TextureHandle normal{};
    TextureHandle material{};
};

struct GBufferTargets {
    TextureHandle albedo{};
    TextureHandle normal{};
    TextureHandle extra{};
    TextureHandle depth{};

    U32 width{0};
    U32 height{0};
};

struct FinalColorTarget {
    TextureHandle color{};

    U32 width{0};
    U32 height{0};
};

struct AmbientOcclusionTargets {
    TextureHandle target{};
    U32 width{0};
    U32 height{0};
};

struct ShadowResources {
    TextureHandle map{};
    U32 size{4096};
};

struct PointShadowResources {
    TextureHandle cube_maps[MAX_POINT_SHADOWS]{};
    U32 size{512};
};

struct SpotShadowResources {
    TextureHandle atlas{};
    U32 size{2048};
    U32 tile_size{1024};
};

struct IblResources {
    TextureHandle source{};

    TextureHandle environment{};
    TextureHandle irradiance{};
    TextureHandle prefiltered{};
    TextureHandle brdf_lut{};

    U32 environment_size{512};
    U32 irradiance_size{32};
    U32 prefiltered_size{128};
    U32 prefiltered_mips{5};
    U32 brdf_lut_size{512};

    bool environment_ready{false};
    bool irradiance_ready{false};
    bool prefiltered_ready{false};
    bool brdf_lut_ready{false};
};

} // namespace fr
