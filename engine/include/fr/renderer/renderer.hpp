/**
 * @file renderer.hpp
 * @author Tfoedy
 * @brief High-level deferred renderer.
 */

#pragma once

#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/renderer/renderer_resources.hpp"

namespace fr {

class Renderer {
public:
    explicit Renderer(RenderDevice *device,
                      const RendererCreateDesc &desc = RendererCreateDesc{}) noexcept;

    ~Renderer() noexcept;

    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer &operator=(Renderer &&) = delete;

    void render(const RenderFrameDesc &desc) noexcept;

    [[nodiscard]] TextureHandle get_final_image() const noexcept;

private:
    void init_global_buffers() noexcept;
    void init_fallback_textures() noexcept;

    void prepare_render_targets(U32 width, U32 height) noexcept;
    void update_global_buffers(const RenderFrameDesc &desc) noexcept;

    void execute_present_pass(CommandBuffer *cmd, RenderPipelineHandle present_pipe) noexcept;

    void execute_ibl_environment_pass(CommandBuffer *cmd, TextureHandle source,
                                      RenderPipelineHandle pipe) noexcept;

    void execute_ibl_irradiance_pass(CommandBuffer *cmd, RenderPipelineHandle pipe) noexcept;

    void execute_ibl_prefilter_pass(CommandBuffer *cmd, RenderPipelineHandle pipe) noexcept;

    void execute_ibl_brdf_lut_pass(CommandBuffer *cmd, RenderPipelineHandle pipe) noexcept;

    void execute_shadow_pass(CommandBuffer *cmd, const RenderQueue &shadow_queue,
                             RenderPipelineHandle shadow_pipe) noexcept;

    void execute_point_shadow_pass(CommandBuffer *cmd, const RenderQueue &shadow_queue,
                                   const RenderQueue &geom_queue,
                                   RenderPipelineHandle point_shadow_pipe) noexcept;

    void execute_spot_shadow_pass(CommandBuffer *cmd, const RenderQueue &shadow_queue,
                                  const RenderQueue &geom_queue,
                                  RenderPipelineHandle spot_shadow_pipe) noexcept;

    void execute_geometry_pass(CommandBuffer *cmd, const RenderQueue &geom_queue, U32 width,
                               U32 height) noexcept;

    void execute_hbao_pass(CommandBuffer *cmd, const RenderFrameDesc &desc) noexcept;

    void execute_lighting_pass(CommandBuffer *cmd, const RenderFrameDesc &desc) noexcept;

    void destroy_global_buffers() noexcept;
    void destroy_fallback_textures() noexcept;
    void destroy_final_color() noexcept;
    void destroy_gbuffer() noexcept;
    void destroy_ao() noexcept;
    void destroy_shadow_resources() noexcept;
    void destroy_point_shadow_resources() noexcept;
    void destroy_spot_shadow_resources() noexcept;
    void destroy_ibl_resources() noexcept;

private:
    RenderDevice *m_device{nullptr};
    RendererLimits m_limits{};

    RendererGlobalBuffers m_global{};
    RendererFallbackTextures m_fallback{};

    FinalColorTarget m_final{};

    GBufferTargets m_gbuffer{};
    AmbientOcclusionTargets m_ao{};

    ShadowResources m_shadow{};
    PointShadowResources m_point_shadows{};
    SpotShadowResources m_spot_shadows{};

    IblResources m_ibl{};
};

} // namespace fr
