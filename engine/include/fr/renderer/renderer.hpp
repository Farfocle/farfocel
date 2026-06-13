/**
 * @file renderer.hpp
 * @author Tfoedy
 * @brief Farfocel's high-level graphics renderer.
 */

#pragma once

#include "fr/core/alloc.hpp"

#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_frame.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/renderer/renderer_pipelines.hpp"
#include "fr/renderer/renderer_resources.hpp"

namespace fr {

/**
 * @brief Owns renderer frame resources and executes built-in passes.
 *
 * @details
 * Renderer consumes RenderFrameDesc data. It does not read ECS state, load assets, or own
 * pipeline handles.
 */
class Renderer {
public:
    explicit Renderer(RenderDevice *device, const RendererCreateDesc &desc) noexcept;

    ~Renderer() noexcept;

    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer &operator=(Renderer &&) = delete;

    /**
     * @brief Renders one frame from a prepared frame description.
     */
    void render(const RenderFrameDesc &desc) noexcept;

    /**
     * @brief Returns the final color target produced by the last rendered frame.
     */
    [[nodiscard]] TextureHandle final_image() const noexcept;

    [[nodiscard]] bool is_ready() const noexcept {
        return m_ready;
    }

    [[nodiscard]] const RendererPipelineSet &pipelines() const noexcept {
        return m_pipelines;
    }

    [[nodiscard]] RenderPipelineHandle geometry_pipeline(bool wireframe = false) const noexcept {
        return wireframe ? m_pipelines.geometry_wire : m_pipelines.geometry;
    }

    [[nodiscard]] RenderPipelineHandle forward_transparent_pipeline() const noexcept {
        return m_pipelines.forward_transparent;
    }

    [[nodiscard]] RenderPipelineHandle shadow_pipeline() const noexcept {
        return m_pipelines.shadow;
    }

    [[nodiscard]] RenderPipelineHandle point_shadow_pipeline() const noexcept {
        return m_pipelines.point_shadow;
    }

    [[nodiscard]] RenderPipelineHandle spot_shadow_pipeline() const noexcept {
        return m_pipelines.spot_shadow;
    }

private:
    void init_global_buffers() noexcept;
    void init_fallback_textures() noexcept;

    void prepare_render_targets(U32 width, U32 height) noexcept;
    void update_global_buffers(const RenderFrameDesc &desc) noexcept;

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
    Alloc *m_alloc{nullptr};

    RendererLimits m_limits{};
    RendererPipelineSet m_pipelines{};

    RendererResources m_resources{};

    bool m_ready{false};
};

} // namespace fr
