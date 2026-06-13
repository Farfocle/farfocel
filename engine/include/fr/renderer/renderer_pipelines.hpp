/**
 * @file renderer_pipelines.hpp
 * @author Tfoedy
 * @brief Pipeline handles used by the built-in renderer.
 */

#pragma once

#include "fr/renderer/render_device.hpp"

namespace fr {

/**
 * @brief Pipeline set consumed by renderer passes.
 *
 * @details
 * Renderer only references these handles. RenderPipelineCache or application setup code owns the
 * actual pipeline resources.
 */
struct RendererPipelineSet {
    RenderPipelineHandle geometry{};
    RenderPipelineHandle geometry_wire{};

    RenderPipelineHandle lighting{};
    RenderPipelineHandle present{};

    RenderPipelineHandle shadow{};
    RenderPipelineHandle point_shadow{};
    RenderPipelineHandle spot_shadow{};

    RenderPipelineHandle hbao{};

    RenderPipelineHandle equirect_to_cube{};
    RenderPipelineHandle irradiance{};
    RenderPipelineHandle prefilter_env{};
    RenderPipelineHandle brdf_lut{};

    /**
     * @brief Returns true when all required renderer pipelines are present.
     */
    [[nodiscard]] bool is_valid() const noexcept {
        return geometry.is_valid() && geometry_wire.is_valid() && lighting.is_valid() &&
               present.is_valid() && shadow.is_valid() && point_shadow.is_valid() &&
               spot_shadow.is_valid() && hbao.is_valid() && equirect_to_cube.is_valid() &&
               irradiance.is_valid() && prefilter_env.is_valid() && brdf_lut.is_valid();
    }
};

} // namespace fr
