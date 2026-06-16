/**
 * @file app.hpp
 * @brief Renderer application bootstrap: window, device, asset pipeline, renderer.
 */

#pragma once

#include <utility>

#include "fr/asset/asset_manager.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/asset/asset_storage.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"
#include "fr/logger/logger.hpp"
#include "fr/platform/window.hpp"
#include "fr/renderer/default_renderer_setup.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_frame.hpp"
#include "fr/renderer/render_pipeline_cache.hpp"
#include "fr/renderer/renderer.hpp"
#include "fr/scene/environment.hpp"
#include "fr/scene/imgui_setup.hpp"
#include "fr/scene/primitive_mesh.hpp"
#include "fr/scene/render_assets.hpp"
#include "fr/scene/transform.hpp"

namespace fr::scene {

struct RendererAppDesc {
    StringView title{"Farfocel App"};
    U32 width{1600};
    U32 height{900};
    bool vsync{true};
};

/// @brief Owns the full rendering infrastructure for an application.
struct RendererApp {
    Alloc *alloc{nullptr};
    Window window{};
    RenderDevice *device{nullptr};
    AssetRegistry *registry{nullptr};
    AssetStorage *storage{nullptr};
    AssetManager *assets{nullptr};
    RenderPipelineCache *pipeline_cache{nullptr};
    Renderer *renderer{nullptr};
    RenderFrameSubmission *submission{nullptr};
    DefaultRendererShaders default_shaders{};
    bool imgui_initialized{false};

    /**
     * @brief Phase 1: creates window, device, imgui, registry, and storage. Call
     * cook_and_register_shaders / load_dev_manifest_if_exists after this, then call
     * init_renderer().
     */
    [[nodiscard]] bool init_platform(const RendererAppDesc &desc) noexcept {
        alloc = fr::get_ambient_ctx().alloc;
        FR_ASSERT(alloc, "ambient allocator must be non-null");

        registry = do_allocate<AssetRegistry>(alloc, alloc);
        storage = do_allocate<AssetStorage>(alloc, alloc);

        WindowProperties props{};
        props.title = desc.title.data();
        props.width = desc.width;
        props.height = desc.height;
        props.vsync = desc.vsync;
        props.api = GRAPHICS_API::OPENGL;

        if (!window.init(props)) {
            FR_LOG_ERR("[RendererApp] Failed to initialize window.");
            return false;
        }

        device = create_opengl_render_device(alloc);
        if (!device) {
            FR_LOG_ERR("[RendererApp] Failed to create render device.");
            shutdown();
            return false;
        }

        ImGuiSetupDesc imgui_desc{};
        imgui_desc.window = &window;
        if (!imgui_init(imgui_desc)) {
            FR_LOG_ERR("[RendererApp] Failed to initialize ImGui.");
            shutdown();
            return false;
        }
        imgui_initialized = true;
        return true;
    }

    /**
     * @brief Phase 2: creates asset manager, pipeline cache, and renderer. Call after
     * init_platform() and any asset cooking/registration.
     */
    [[nodiscard]] bool init_renderer() noexcept {
        assets = do_allocate<AssetManager>(alloc, device, alloc, registry, storage);
        pipeline_cache = do_allocate<RenderPipelineCache>(alloc, device, assets, alloc);

        DefaultRendererShaderIds shader_ids{};
        if (!load_default_renderer_shaders(*assets, shader_ids, default_shaders)) {
            FR_LOG_ERR("[RendererApp] Failed to load renderer shaders.");
            shutdown();
            return false;
        }

        RendererPipelineSet pipelines{};
        if (!create_default_renderer_pipelines(*pipeline_cache, default_shaders, pipelines)) {
            FR_LOG_ERR("[RendererApp] Failed to create renderer pipelines.");
            shutdown();
            return false;
        }

        RendererCreateDesc renderer_desc{};
        renderer_desc.alloc = alloc;
        renderer_desc.pipelines = pipelines;

        renderer = do_allocate<Renderer>(alloc, device, renderer_desc);
        if (!renderer->is_ready()) {
            FR_LOG_ERR("[RendererApp] Renderer failed to initialize.");
            shutdown();
            return false;
        }

        submission = do_allocate<RenderFrameSubmission>(alloc, alloc);
        return true;
    }

    void shutdown() noexcept {
        if (assets && default_shaders.is_valid()) {
            unload_default_renderer_shaders(*assets, default_shaders);
        }

        do_deallocate(submission);
        do_deallocate(renderer);
        do_deallocate(pipeline_cache);
        do_deallocate(assets);
        do_deallocate(storage);
        do_deallocate(registry);

        if (imgui_initialized) {
            imgui_shutdown(window);
            imgui_initialized = false;
        }

        if (device) {
            destroy_opengl_render_device(device);
            device = nullptr;
        }

        window.close();
    }

    /// @brief Runs per-frame scene resolution: transforms, meshes, assets, environment, uploads.
    void run(World &world, USize async_budget = 8) noexcept {
        rebuild_world_transforms(world);
        resolve_primitive_meshes(world, *assets, alloc);
        resolve_render_assets(world, *assets);
        resolve_environment(world, *assets);
        assets->process_async_uploads(async_budget);
    }

private:
    template <typename T, typename... Args>
    T *do_allocate(Alloc *a, Args &&...args) noexcept {
        return std::construct_at(static_cast<T *>(a->allocate(sizeof(T), alignof(T))),
                                 std::forward<Args>(args)...);
    }

    template <typename T>
    void do_deallocate(T *&ptr) noexcept {
        if (ptr) {
            std::destroy_at(ptr);
            alloc->deallocate(ptr, sizeof(T), alignof(T));
            ptr = nullptr;
        }
    }
};

} // namespace fr::scene
