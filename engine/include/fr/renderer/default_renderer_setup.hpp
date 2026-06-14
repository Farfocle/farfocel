/**
 * @file default_renderer_setup.hpp
 * @author Tfoedy
 * @brief Built-in renderer shader and pipeline setup.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_manager.hpp"
#include "fr/logger/logger.hpp"
#include "fr/renderer/render_pipeline_cache.hpp"
#include "fr/renderer/renderer_pipelines.hpp"

namespace fr {

/**
 * @brief Logical shader asset ids used by the built-in renderer.
 */
struct DefaultRendererShaderIds {
    AssetId gbuffer_shader{FR_ASSET_ID("renderer.shader.gbuffer")};
    AssetId forward_transparent_shader{FR_ASSET_ID("renderer.shader.forward_transparent")};
    AssetId lighting_shader{FR_ASSET_ID("renderer.shader.lighting")};

    AssetId shadow_shader{FR_ASSET_ID("renderer.shader.shadow")};
    AssetId point_shadow_shader{FR_ASSET_ID("renderer.shader.point_shadow")};
    AssetId spot_shadow_shader{FR_ASSET_ID("renderer.shader.spot_shadow")};

    AssetId hbao_shader{FR_ASSET_ID("renderer.shader.hbao")};

    AssetId equirect_to_cube_shader{FR_ASSET_ID("renderer.shader.equirect_to_cube")};
    AssetId irradiance_shader{FR_ASSET_ID("renderer.shader.irradiance")};
    AssetId prefilter_env_shader{FR_ASSET_ID("renderer.shader.prefilter_env")};
    AssetId brdf_lut_shader{FR_ASSET_ID("renderer.shader.brdf_lut")};

    AssetId present_shader{FR_ASSET_ID("renderer.shader.present")};
};

/**
 * @brief Loaded shader assets used by the built-in renderer.
 */
struct DefaultRendererShaders {
    ShaderAssetHandle gbuffer{};
    ShaderAssetHandle forward_transparent{};
    ShaderAssetHandle lighting{};

    ShaderAssetHandle shadow{};
    ShaderAssetHandle point_shadow{};
    ShaderAssetHandle spot_shadow{};

    ShaderAssetHandle hbao{};

    ShaderAssetHandle equirect_to_cube{};
    ShaderAssetHandle irradiance{};
    ShaderAssetHandle prefilter_env{};
    ShaderAssetHandle brdf_lut{};

    ShaderAssetHandle present{};

    [[nodiscard]] bool is_valid() const noexcept {
        return gbuffer.is_valid() && forward_transparent.is_valid() && lighting.is_valid() &&
               shadow.is_valid() && point_shadow.is_valid() && spot_shadow.is_valid() &&
               hbao.is_valid() && equirect_to_cube.is_valid() && irradiance.is_valid() &&
               prefilter_env.is_valid() && brdf_lut.is_valid() && present.is_valid();
    }
};

/**
 * @brief Unloads built-in renderer shader assets.
 */
inline void unload_default_renderer_shaders(AssetManager &assets,
                                            DefaultRendererShaders &shaders) noexcept {
    assets.unload_shader(shaders.present);
    assets.unload_shader(shaders.brdf_lut);
    assets.unload_shader(shaders.prefilter_env);
    assets.unload_shader(shaders.irradiance);
    assets.unload_shader(shaders.equirect_to_cube);
    assets.unload_shader(shaders.hbao);
    assets.unload_shader(shaders.spot_shadow);
    assets.unload_shader(shaders.point_shadow);
    assets.unload_shader(shaders.shadow);
    assets.unload_shader(shaders.lighting);
    assets.unload_shader(shaders.forward_transparent);
    assets.unload_shader(shaders.gbuffer);

    shaders = {};
}

/**
 * @brief Loads built-in renderer shader assets.
 */
inline bool load_default_renderer_shaders(AssetManager &assets, const DefaultRendererShaderIds &ids,
                                          DefaultRendererShaders &out_shaders) noexcept {
    out_shaders = {};

    out_shaders.gbuffer = assets.load_shader(ids.gbuffer_shader);
    out_shaders.forward_transparent = assets.load_shader(ids.forward_transparent_shader);
    out_shaders.lighting = assets.load_shader(ids.lighting_shader);

    out_shaders.shadow = assets.load_shader(ids.shadow_shader);
    out_shaders.point_shadow = assets.load_shader(ids.point_shadow_shader);
    out_shaders.spot_shadow = assets.load_shader(ids.spot_shadow_shader);

    out_shaders.hbao = assets.load_shader(ids.hbao_shader);

    out_shaders.equirect_to_cube = assets.load_shader(ids.equirect_to_cube_shader);
    out_shaders.irradiance = assets.load_shader(ids.irradiance_shader);
    out_shaders.prefilter_env = assets.load_shader(ids.prefilter_env_shader);
    out_shaders.brdf_lut = assets.load_shader(ids.brdf_lut_shader);

    out_shaders.present = assets.load_shader(ids.present_shader);

    if (!out_shaders.is_valid()) {
        FR_LOG_ERR("Failed to load built-in renderer shaders.");
        unload_default_renderer_shaders(assets, out_shaders);
        return false;
    }

    return true;
}

/**
 * @brief Creates built-in renderer pipelines.
 */
inline bool create_default_renderer_pipelines(RenderPipelineCache &pipeline_cache,
                                              const DefaultRendererShaders &shaders,
                                              RendererPipelineSet &out_pipelines) noexcept {
    if (!shaders.is_valid()) {
        FR_LOG_ERR("Cannot create renderer pipelines from invalid shader set.");
        out_pipelines = {};
        return false;
    }

    out_pipelines = {};

    out_pipelines.geometry = pipeline_cache.get_or_create({
        .shader = shaders.gbuffer,
        .cull_mode = CullMode::Back,
        .blend_mode = BlendMode::None,
        .depth_test = true,
        .depth_write = true,
        .wireframe = false,
    });

    out_pipelines.geometry_wire = pipeline_cache.get_or_create({
        .shader = shaders.gbuffer,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = true,
        .depth_write = true,
        .wireframe = true,
    });

    out_pipelines.forward_transparent = pipeline_cache.get_or_create({
        .shader = shaders.forward_transparent,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::Alpha,
        .depth_test = true,
        .depth_write = false,
        .wireframe = false,
    });

    out_pipelines.lighting = pipeline_cache.get_or_create({
        .shader = shaders.lighting,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = false,
        .depth_write = false,
        .wireframe = false,
    });

    out_pipelines.present = pipeline_cache.get_or_create({
        .shader = shaders.present,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = false,
        .depth_write = false,
        .wireframe = false,
    });

    out_pipelines.shadow = pipeline_cache.get_or_create({
        .shader = shaders.shadow,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = true,
        .depth_write = true,
        .wireframe = false,
    });

    out_pipelines.point_shadow = pipeline_cache.get_or_create({
        .shader = shaders.point_shadow,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = true,
        .depth_write = true,
        .wireframe = false,
    });

    out_pipelines.spot_shadow = pipeline_cache.get_or_create({
        .shader = shaders.spot_shadow,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = true,
        .depth_write = true,
        .wireframe = false,
    });

    out_pipelines.hbao = pipeline_cache.get_or_create({
        .shader = shaders.hbao,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = false,
        .depth_write = false,
        .wireframe = false,
    });

    out_pipelines.equirect_to_cube = pipeline_cache.get_or_create({
        .shader = shaders.equirect_to_cube,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = false,
        .depth_write = false,
        .wireframe = false,
    });

    out_pipelines.irradiance = pipeline_cache.get_or_create({
        .shader = shaders.irradiance,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = false,
        .depth_write = false,
        .wireframe = false,
    });

    out_pipelines.prefilter_env = pipeline_cache.get_or_create({
        .shader = shaders.prefilter_env,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = false,
        .depth_write = false,
        .wireframe = false,
    });

    out_pipelines.brdf_lut = pipeline_cache.get_or_create({
        .shader = shaders.brdf_lut,
        .cull_mode = CullMode::None,
        .blend_mode = BlendMode::None,
        .depth_test = false,
        .depth_write = false,
        .wireframe = false,
    });

    if (!out_pipelines.is_valid()) {
        FR_LOG_ERR("Failed to create built-in renderer pipelines.");
        out_pipelines = {};
        return false;
    }

    return true;
}

} // namespace fr
