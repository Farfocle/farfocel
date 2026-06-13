/**
 * @file render_pipeline_cache.hpp
 * @author Tfoedy
 * @brief Cache for renderer pipeline handles.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/hash_map.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_manager.hpp"
#include "fr/logger/logger.hpp"
#include "fr/renderer/render_device.hpp"

namespace fr {

/**
 * @brief Pipeline state requested from a shader asset.
 */
struct RenderPipelineAssetDesc {
    ShaderAssetHandle shader{};

    CullMode cull_mode{CullMode::Back};
    BlendMode blend_mode{BlendMode::None};

    bool depth_test{true};
    bool depth_write{true};
    bool wireframe{false};
};

/**
 * @brief Owns render pipelines created from shader assets.
 *
 * @details
 * Shader assets are owned by AssetManager and must outlive all pipelines created from them.
 */
class RenderPipelineCache {
public:
    RenderPipelineCache(RenderDevice *device, const AssetManager *assets, Alloc *alloc) noexcept
        : m_device(device),
          m_assets(assets),
          m_cache(HashMap<U64, RenderPipelineHandle>::with_capacity(alloc, 256)),
          m_pipelines(alloc) {
        FR_ASSERT(device, "RenderDevice must be non-null");
        FR_ASSERT(assets, "AssetManager must be non-null");
        FR_ASSERT(alloc, "allocator must be non-null");

        m_pipelines.reserve(256);
    }

    ~RenderPipelineCache() noexcept {
        clear();
    }

    RenderPipelineCache(const RenderPipelineCache &) = delete;
    RenderPipelineCache(RenderPipelineCache &&) = delete;
    RenderPipelineCache &operator=(const RenderPipelineCache &) = delete;
    RenderPipelineCache &operator=(RenderPipelineCache &&) = delete;

    /**
     * @brief Returns a cached pipeline or creates it.
     */
    [[nodiscard]] RenderPipelineHandle get_or_create(const RenderPipelineAssetDesc &desc) noexcept {
        if (!desc.shader.is_valid()) {
            FR_LOG_ERR("Cannot create render pipeline from invalid shader asset handle.");
            return {};
        }

        const U64 key = make_pipeline_key(desc);

        if (auto cached = m_cache.find(key); cached.is_some()) {
            return *cached.unwrap();
        }

        ShaderHandle shader = m_assets->get_shader_handle(desc.shader);
        if (!shader.is_valid()) {
            FR_LOG_ERR("Cannot create render pipeline because shader asset is not loaded.");
            return {};
        }

        RenderPipelineProperties props{};
        props.shader = shader;
        props.cull_mode = desc.cull_mode;
        props.depth_test = desc.depth_test;
        props.depth_write = desc.depth_write;
        props.wireframe = desc.wireframe;
        props.blend_mode = desc.blend_mode;

        RenderPipelineHandle pipeline = m_device->create_render_pipeline(props);
        if (!pipeline.is_valid()) {
            FR_LOG_ERR("RenderDevice failed to create render pipeline.");
            return {};
        }

        m_cache.insert(key, pipeline);
        m_pipelines.push_back(pipeline);

        return pipeline;
    }

    /**
     * @brief Destroys all cached pipelines.
     */
    void clear() noexcept {
        for (RenderPipelineHandle pipeline : m_pipelines) {
            if (pipeline.is_valid()) {
                m_device->destroy_pipeline(pipeline);
            }
        }

        m_pipelines.clear();
        m_cache.clear();
    }

private:
    /**
     * @brief Builds a compact key from shader asset handle and fixed-function state.
     */
    static U64 make_pipeline_key(const RenderPipelineAssetDesc &desc) noexcept {
        U64 key = 0;

        key |= (static_cast<U64>(desc.shader.key.index) & 0xFFFFULL) << 48;
        key |= (static_cast<U64>(desc.shader.key.generation) & 0xFFFFULL) << 32;

        key |= (static_cast<U64>(desc.cull_mode) & 0xFULL) << 24;
        key |= (static_cast<U64>(desc.blend_mode) & 0xFULL) << 20;

        key |= desc.depth_test ? (1ULL << 3) : 0ULL;
        key |= desc.depth_write ? (1ULL << 2) : 0ULL;
        key |= desc.wireframe ? (1ULL << 1) : 0ULL;

        return key;
    }

private:
    RenderDevice *m_device{nullptr};
    const AssetManager *m_assets{nullptr};

    HashMap<U64, RenderPipelineHandle> m_cache;
    DynamicArray<RenderPipelineHandle> m_pipelines;
};

} // namespace fr
