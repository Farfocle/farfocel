/**
 * @file render_passes.hpp
 * @author Tfoedy
 * @brief Built-in renderer pass entry points.
 */

#pragma once

#include "fr/core/typedefs.hpp"

#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_frame.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/renderer/renderer_pipelines.hpp"
#include "fr/renderer/renderer_resources.hpp"

namespace fr::render_pass {

struct IblEnvironmentPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};

    TextureHandle source{};
};

struct IblIrradiancePassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};
};

struct IblPrefilterPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};
};

struct IblBrdfLutPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};
};

struct DirectionalShadowPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};
    const RenderFrameSubmission *submission{nullptr};
};

struct PointShadowPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};
    const RenderFrameSubmission *submission{nullptr};

    RendererLimits limits{};
};

struct SpotShadowPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};
    const RenderFrameSubmission *submission{nullptr};

    RendererLimits limits{};
};

struct GeometryPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};
    const RenderFrameSubmission *submission{nullptr};

    U32 width{0};
    U32 height{0};
};

struct HbaoPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};

    const RenderFrameDesc *frame{nullptr};
};

struct LightingPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};

    const RenderFrameDesc *frame{nullptr};
    RendererLimits limits{};
};

/**
 * @brief Forward transparent pass input.
 */
struct ForwardTransparentPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};

    const RenderFrameDesc *frame{nullptr};
    RendererLimits limits{};
};

struct PresentPassDesc {
    RendererResources *resources{nullptr};
    const RendererPipelineSet *pipelines{nullptr};
};

void execute_ibl_environment(CommandBuffer *cmd, const IblEnvironmentPassDesc &desc) noexcept;

void execute_ibl_irradiance(CommandBuffer *cmd, const IblIrradiancePassDesc &desc) noexcept;

void execute_ibl_prefilter(CommandBuffer *cmd, const IblPrefilterPassDesc &desc) noexcept;

void execute_ibl_brdf_lut(CommandBuffer *cmd, const IblBrdfLutPassDesc &desc) noexcept;

void execute_directional_shadow(CommandBuffer *cmd, const DirectionalShadowPassDesc &desc) noexcept;

void execute_point_shadow(CommandBuffer *cmd, const PointShadowPassDesc &desc) noexcept;

void execute_spot_shadow(CommandBuffer *cmd, const SpotShadowPassDesc &desc) noexcept;

void execute_geometry(CommandBuffer *cmd, const GeometryPassDesc &desc) noexcept;

void execute_hbao(CommandBuffer *cmd, const HbaoPassDesc &desc) noexcept;

void execute_lighting(CommandBuffer *cmd, const LightingPassDesc &desc) noexcept;

void execute_forward_transparent(CommandBuffer *cmd,
                                 const ForwardTransparentPassDesc &desc) noexcept;

void execute_present(CommandBuffer *cmd, const PresentPassDesc &desc) noexcept;

} // namespace fr::render_pass
