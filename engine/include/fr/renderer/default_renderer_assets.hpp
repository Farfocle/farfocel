/**
 * @file default_renderer_assets.hpp
 * @author Tfoedy
 * @brief Built-in renderer asset registration helpers.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_kind.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/logger/logger.hpp"

namespace fr {

/**
 * @brief Registers built-in renderer shaders as loose cooked assets.
 *
 * @details
 * Used by development setups before manifests or packs are mounted.
 */
inline bool register_default_renderer_loose_assets(AssetRegistry &registry) noexcept {
    bool ok = true;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.gbuffer"), AssetKind::Shader,
                                       "assets/shaders/core/gbuffer.fshader") &&
         ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.lighting"), AssetKind::Shader,
                                       "assets/shaders/core/lighting.fshader") &&
         ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.shadow"), AssetKind::Shader,
                                       "assets/shaders/core/shadow.fshader") &&
         ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.point_shadow"),
                                       AssetKind::Shader,
                                       "assets/shaders/core/point_shadow.fshader") &&
         ok;

    ok =
        registry.register_loose_asset(FR_ASSET_ID("renderer.shader.spot_shadow"), AssetKind::Shader,
                                      "assets/shaders/core/spot_shadow.fshader") &&
        ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.hbao"), AssetKind::Shader,
                                       "assets/shaders/core/hbao.fshader") &&
         ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.equirect_to_cube"),
                                       AssetKind::Shader,
                                       "assets/shaders/core/equirect_to_cube.fshader") &&
         ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.irradiance"), AssetKind::Shader,
                                       "assets/shaders/core/irradiance_convolution.fshader") &&
         ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.prefilter_env"),
                                       AssetKind::Shader,
                                       "assets/shaders/core/prefilter_env.fshader") &&
         ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.brdf_lut"), AssetKind::Shader,
                                       "assets/shaders/core/brdf_lut.fshader") &&
         ok;

    ok = registry.register_loose_asset(FR_ASSET_ID("renderer.shader.present"), AssetKind::Shader,
                                       "assets/shaders/core/present.fshader") &&
         ok;

    if (!ok) {
        FR_LOG_ERR("Failed to register built-in renderer loose assets.");
    }

    return ok;
}

} // namespace fr
