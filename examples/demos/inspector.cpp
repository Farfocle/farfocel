/**
 * @file devtools_demo.cpp
 * @author Tfoedy
 * @brief Runtime devtools demo with glTF import, ECS inspector and renderer integration.
 */

#include <chrono>
#include <cstdlib>
#include <utility>

#include <SDL3/SDL.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <glm/gtc/quaternion.hpp>

#include "fr/asscooker/asscooker.hpp"
#include "fr/asscooker/dev_asset_catalog.hpp"
#include "fr/asscooker/imgui/gltf_import_panel.hpp"

#include "fr/asset/asset_manager.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/asset/asset_storage.hpp"

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"

#include "fr/devtools/devtools_state.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/devtools/imgui/spawn_panel.hpp"
#include "fr/devtools/inspector.hpp"
#include "fr/devtools/scene_io.hpp"
#include "fr/devtools/world_actions.hpp"

#include "fr/logger/logger.hpp"
#include "fr/logger/sinks/standard_sink.hpp"

#include "fr/platform/input.hpp"
#include "fr/platform/keycode.hpp"
#include "fr/platform/window.hpp"

#include "fr/renderer/default_renderer_setup.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_pipeline_cache.hpp"
#include "fr/renderer/renderer.hpp"

#include "fr/scene/render_asset_system.hpp"
#include "fr/scene/render_extractor.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/transform_system.hpp"

#include "fr/asset/asset_manifest.hpp"

namespace {

constexpr USize ASYNC_UPLOAD_BUDGET_PER_FRAME = 8;
constexpr F32 CAMERA_PRECISION_SPEED_SCALE = 0.15f;
constexpr fr::MouseButton CAMERA_MOUSE_BUTTON = static_cast<fr::MouseButton>(3);

struct DefaultShaderCookInput {
    fr::AssetId id{};
    fr::StringView vertex_path{};
    fr::StringView fragment_path{};
    fr::StringView output_path{};
};

/**
 * @brief Non-owning runtime services exposed to ECS devtools systems.
 */
struct DevToolsRuntimeResource {
    fr::Alloc *alloc{nullptr};

    fr::AssetRegistry *registry{nullptr};
    fr::AssetStorage *storage{nullptr};
    fr::AssetManager *assets{nullptr};

    fr::asscooker::DevAssetCatalog *asset_catalog{nullptr};

    fr::devtools::DevToolsState *tools{nullptr};
    fr::devtools::SpawnPanelState *spawn_panel{nullptr};
    fr::asscooker::imgui::GltfImportPanelState *gltf_import_panel{nullptr};

    template <typename Archive>
    void shape(Archive &) noexcept {
        // Runtime-only non-owning pointers. Intentionally not serialized.
    }
};

/**
 * @brief Per-frame input state exposed to ECS systems.
 */
struct DemoFrameResource {
    fr::Window *window{nullptr};
    fr::WindowInput *input{nullptr};

    F32 dt{0.0f};
    bool camera_active{false};

    template <typename Archive>
    void shape(Archive &) noexcept {
        // Runtime-only frame state. Intentionally not serialized.
    }
};

} // namespace

FR_TYPE(DevToolsRuntimeResource);
FR_TYPE(DemoFrameResource);

namespace {

void setup_logger() {
    auto standard_sink = fr::make_unique<fr::StandardSink>(fr::StandardSink::Options{});
    fr::get_ambient_ctx().logger->add_sink(std::move(standard_sink));
}

static void imgui_event_callback(void *event_data, void *) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event *>(event_data));
}

bool init_imgui(fr::Window &window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    SDL_Window *sdl_window = static_cast<SDL_Window *>(window.get_native_window());
    SDL_GLContext gl_context = static_cast<SDL_GLContext>(window.get_native_context());

    if (!ImGui_ImplSDL3_InitForOpenGL(sdl_window, gl_context)) {
        FR_LOG_ERR("[DevToolsDemo] Failed to initialize ImGui SDL3 backend.");
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 450")) {
        FR_LOG_ERR("[DevToolsDemo] Failed to initialize ImGui OpenGL backend.");
        return false;
    }

    window.set_event_callback(imgui_event_callback, nullptr);
    return true;
}

void shutdown_imgui(fr::Window &window) {
    window.set_event_callback(nullptr, nullptr);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

bool register_cooked_outputs(fr::AssetRegistry &registry,
                             fr::Slice<const fr::asscooker::CookedAssetOutput> outputs) noexcept {
    bool ok = true;

    for (const fr::asscooker::CookedAssetOutput &output : outputs) {
        if (!output.id.is_valid() || output.kind == fr::AssetKind::Unknown ||
            output.path.size() == 0) {
            FR_LOG_ERR("[DevToolsDemo] Invalid cooked output record.");
            ok = false;
            continue;
        }

        ok = registry.register_loose_asset(output.id, output.kind, output.path.view(),
                                           output.content_hash) &&
             ok;
    }

    return ok;
}

bool cook_default_renderer_shaders(fr::Alloc *alloc, fr::AssetRegistry &registry) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    fr::DynamicArray<fr::asscooker::CookedAssetOutput> outputs(alloc);

    const DefaultShaderCookInput shaders[] = {
        {FR_ASSET_ID("renderer.shader.gbuffer"), "engine/shaders/core/gbuffer.vert",
         "engine/shaders/core/gbuffer.frag", "assets/shaders/core/gbuffer.fshader"},

        {FR_ASSET_ID("renderer.shader.forward_transparent"),
         "engine/shaders/core/forward_transparent.vert",
         "engine/shaders/core/forward_transparent.frag",
         "assets/shaders/core/forward_transparent.fshader"},

        {FR_ASSET_ID("renderer.shader.lighting"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/lighting.frag", "assets/shaders/core/lighting.fshader"},

        {FR_ASSET_ID("renderer.shader.shadow"), "engine/shaders/core/shadow.vert",
         "engine/shaders/core/shadow.frag", "assets/shaders/core/shadow.fshader"},

        {FR_ASSET_ID("renderer.shader.point_shadow"), "engine/shaders/core/point_shadow.vert",
         "engine/shaders/core/point_shadow.frag", "assets/shaders/core/point_shadow.fshader"},

        {FR_ASSET_ID("renderer.shader.spot_shadow"), "engine/shaders/core/spot_shadow.vert",
         "engine/shaders/core/spot_shadow.frag", "assets/shaders/core/spot_shadow.fshader"},

        {FR_ASSET_ID("renderer.shader.hbao"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/hbao.frag", "assets/shaders/core/hbao.fshader"},

        {FR_ASSET_ID("renderer.shader.equirect_to_cube"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/equirect_to_cube.frag",
         "assets/shaders/core/equirect_to_cube.fshader"},

        {FR_ASSET_ID("renderer.shader.irradiance"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/irradiance_convolution.frag",
         "assets/shaders/core/irradiance_convolution.fshader"},

        {FR_ASSET_ID("renderer.shader.prefilter_env"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/prefilter_env.frag", "assets/shaders/core/prefilter_env.fshader"},

        {FR_ASSET_ID("renderer.shader.brdf_lut"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/brdf_lut.frag", "assets/shaders/core/brdf_lut.fshader"},

        {FR_ASSET_ID("renderer.shader.present"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/present.frag", "assets/shaders/core/present.fshader"},
    };

    for (const DefaultShaderCookInput &shader : shaders) {
        fr::asscooker::CookOptions options{};
        options.output_id = shader.id;
        options.force = true;

        if (!fr::asscooker::cook_shader_ex(shader.vertex_path, shader.fragment_path,
                                           shader.output_path, &outputs, options)) {
            FR_LOG_ERR("[DevToolsDemo] Failed to cook renderer shader: {}", shader.output_path);
            return false;
        }
    }

    return register_cooked_outputs(registry, outputs.slice());
}

bool load_dev_asset_manifest_if_exists(fr::Alloc *alloc, fr::AssetRegistry &registry,
                                       fr::AssetStorage &storage,
                                       fr::StringView manifest_path) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (manifest_path.is_empty()) {
        return true;
    }

    fr::String path = fr::String::from_view(alloc, manifest_path);

    if (!fr::file::exists(path)) {
        FR_LOG("[DevToolsDemo] Development asset manifest not found: {}", manifest_path);
        return true;
    }

    if (!fr::load_asset_manifest(alloc, manifest_path, registry, storage)) {
        FR_LOG_ERR("[DevToolsDemo] Failed to load development asset manifest: {}", manifest_path);
        return false;
    }

    FR_LOG_OK("[DevToolsDemo] Loaded development asset manifest: {}", manifest_path);
    return true;
}

void init_devtools_defaults(fr::devtools::DevToolsState &tools) noexcept {
    tools.lighting.exposure = 1.0f;
    tools.lighting.pbr_ambient_strength = 0.03f;
    tools.lighting.standard_ambient_strength = 0.035f;
    tools.lighting.standard_specular_default = 0.25f;

    tools.ao.enabled = false;
    tools.ao.radius = 1.5f;
    tools.ao.intensity = 1.2f;
    tools.ao.bias = 0.05f;
    tools.ao.power = 1.5f;
    tools.ao.thickness = 1.0f;

    tools.ibl.enabled = false;
    tools.ibl.diffuse_strength = 0.10f;
    tools.ibl.specular_strength = 1.0f;
    tools.ibl.occlusion_strength = 1.0f;
    tools.ibl.occlusion_power = 2.0f;
    tools.ibl.sky_visibility_strength = 0.75f;

    tools.debug.mode = fr::RenderDebugMode::Final;
    tools.debug.flags = 0;
}

void setup_default_scene(fr::devtools::EditorContext &ctx) noexcept {
    fr::devtools::ensure_default_scene_part_types(*ctx.world);

    fr::devtools::SpawnTransformDesc camera_transform{};
    camera_transform.position = fr::Vec3(0.0f, 2.0f, 7.0f);
    camera_transform.rotation = glm::quat(glm::radians(fr::Vec3(-10.0f, 180.0f, 0.0f)));

    fr::Thing camera = fr::devtools::spawn_camera(ctx, camera_transform);

    if (fr::FPSControllerPart *fps = ctx.world->try_get<fr::FPSControllerPart>(camera)) {
        fps->pitch = -10.0f;
        fps->yaw = 180.0f;
        fps->move_speed = 15.0f;
        fps->mouse_sensitivity = 0.1f;
    }

    fr::devtools::SpawnTransformDesc sun_transform{};
    sun_transform.rotation = glm::quat(glm::radians(fr::Vec3(-60.0f, 30.0f, 0.0f)));

    fr::Thing sun_entity = fr::devtools::spawn_directional_light(ctx, sun_transform);
    if (fr::DirectionalLightPart *sun = ctx.world->try_get<fr::DirectionalLightPart>(sun_entity)) {
        sun->color = fr::Vec3(1.0f, 0.95f, 0.9f);
        sun->intensity = 4.0f;
    }

    fr::devtools::SpawnTransformDesc point_transform{};
    point_transform.position = fr::Vec3(3.0f, 3.0f, 2.0f);

    fr::Thing point_entity = fr::devtools::spawn_point_light(ctx, point_transform);
    if (fr::PointLightPart *point = ctx.world->try_get<fr::PointLightPart>(point_entity)) {
        point->color = fr::Vec3(1.0f, 0.82f, 0.6f);
        point->intensity = 10.0f;
        point->radius = 24.0f;
        point->casts_shadow = false;
    }

    fr::TransformSystem::rebuild_world_transforms(*ctx.world);
    fr::devtools::clear_selection(ctx);
}

static void camera_control_system(fr::Scope scope) {
    DemoFrameResource &frame = scope.get_resource<DemoFrameResource>();

    if (!frame.window || !frame.input) {
        return;
    }

    fr::Window &window = *frame.window;
    fr::WindowInput &input = *frame.input;

    ImGuiIO &io = ImGui::GetIO();

    const bool camera_should_be_active =
        window.is_focused() && input.is_mouse_down(CAMERA_MOUSE_BUTTON) && !io.WantCaptureMouse;

    if (camera_should_be_active != frame.camera_active) {
        frame.camera_active = camera_should_be_active;
        window.set_mouse_mode(frame.camera_active ? fr::MouseMode::Relative
                                                  : fr::MouseMode::Normal);
    }

    if (!frame.camera_active) {
        return;
    }

    const bool precision = input.is_key_down(fr::Key::LShift);

    for (auto [thing, fps, local] : scope.query<fr::FPSControllerPart, fr::LocalTransformPart>()) {
        (void)thing;

        fps.yaw -= input.mouse_delta_x * fps.mouse_sensitivity;
        fps.pitch -= input.mouse_delta_y * fps.mouse_sensitivity;
        fps.pitch = glm::clamp(fps.pitch, -89.0f, 89.0f);

        local.rotation = glm::quat(glm::vec3(glm::radians(fps.pitch), glm::radians(fps.yaw), 0.0f));

        const fr::Vec3 forward = local.rotation * fr::Vec3(0.0f, 0.0f, -1.0f);
        const fr::Vec3 right = local.rotation * fr::Vec3(1.0f, 0.0f, 0.0f);
        const fr::Vec3 up = fr::Vec3(0.0f, 1.0f, 0.0f);

        F32 speed = fps.move_speed * frame.dt;
        if (precision) {
            speed *= CAMERA_PRECISION_SPEED_SCALE;
        }

        if (input.is_key_down(fr::Key::W)) {
            local.position += forward * speed;
        }

        if (input.is_key_down(fr::Key::S)) {
            local.position -= forward * speed;
        }

        if (input.is_key_down(fr::Key::A)) {
            local.position -= right * speed;
        }

        if (input.is_key_down(fr::Key::D)) {
            local.position += right * speed;
        }

        if (input.is_key_down(fr::Key::Space)) {
            local.position += up * speed;
        }
    }

    fr::TransformSystem::rebuild_world_transforms(scope.world());
}

static void devtools_ui_system(fr::Scope scope) {
    DevToolsRuntimeResource &runtime = scope.get_resource<DevToolsRuntimeResource>();

    FR_ASSERT(runtime.assets, "AssetManager must be available");
    FR_ASSERT(runtime.registry, "AssetRegistry must be available");
    FR_ASSERT(runtime.asset_catalog, "DevAssetCatalog must be available");
    FR_ASSERT(runtime.tools, "DevToolsState must be available");
    FR_ASSERT(runtime.spawn_panel, "SpawnPanelState must be available");
    FR_ASSERT(runtime.gltf_import_panel, "GltfImportPanelState must be available");

    fr::devtools::EditorContext editor_ctx{};
    editor_ctx.world = &scope.world();
    editor_ctx.assets = runtime.assets;
    editor_ctx.tools = runtime.tools;

    if (runtime.tools->show_inspector) {
        ImGui::SetNextWindowSize(ImVec2(1100.0f, 660.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("World Inspector", &runtime.tools->show_inspector)) {
            fr::devtools::inspector(editor_ctx);
        }

        ImGui::End();
    }

    if (runtime.tools->show_spawn_panel) {
        ImGui::SetNextWindowSize(ImVec2(440.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(1140.0f, 20.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Spawn / Scene", &runtime.tools->show_spawn_panel)) {
            fr::devtools::spawn_panel(editor_ctx, *runtime.spawn_panel);
        }

        ImGui::End();
    }

    ImGui::SetNextWindowSize(ImVec2(520.0f, 340.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(1140.0f, 560.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Asset Import")) {
        fr::asscooker::DevAssetImportContext import_ctx{};
        import_ctx.alloc = runtime.alloc;
        import_ctx.registry = runtime.registry;
        import_ctx.catalog = runtime.asset_catalog;

        [[maybe_unused]] fr::asscooker::ImportedModelResult imported =
            fr::asscooker::imgui::draw_gltf_import_panel(import_ctx, editor_ctx,
                                                         *runtime.gltf_import_panel);
    }

    ImGui::End();
}

fr::RenderFrameDesc build_frame_desc(const fr::RenderExtractResult &extract_result,
                                     const fr::RenderFrameSubmission &submission, U32 width,
                                     U32 height,
                                     const fr::devtools::DevToolsState &tools) noexcept {
    fr::RenderFrameDesc frame{};
    frame.submission = &submission;

    frame.viewport.width = width;
    frame.viewport.height = height;

    frame.camera = extract_result.camera;
    frame.environment_source = {};

    frame.lighting = tools.lighting;
    frame.ao = tools.ao;
    frame.ibl = tools.ibl;
    frame.debug = tools.debug;

    return frame;
}

} // namespace

S32 main(S32 argc, char **argv) {
    (void)argc;
    (void)argv;

    fr::init_core_ctx();
    setup_logger();

    S32 exit_code = EXIT_SUCCESS;

    {
        fr::Alloc *alloc = fr::get_ambient_ctx().alloc;
        FR_ASSERT(alloc, "ambient allocator must be non-null");

        FR_LOG("[DevToolsDemo] Starting runtime devtools demo.");

        fr::Window window{};
        fr::RenderDevice *device = nullptr;
        bool imgui_initialized = false;

        fr::WindowProperties props{};
        props.title = "Farfocel DevTools Demo";
        props.width = 1600;
        props.height = 900;
        props.vsync = true;
        props.fullscreen = false;
        props.api = fr::GRAPHICS_API::OPENGL;

        if (!window.init(props)) {
            FR_LOG_ERR("[DevToolsDemo] Failed to initialize window.");
            exit_code = EXIT_FAILURE;
        }

        if (exit_code == EXIT_SUCCESS) {
            device = fr::create_opengl_render_device(alloc);
            if (!device) {
                FR_LOG_ERR("[DevToolsDemo] Failed to create OpenGL render device.");
                exit_code = EXIT_FAILURE;
            }
        }

        if (exit_code == EXIT_SUCCESS) {
            imgui_initialized = init_imgui(window);
            if (!imgui_initialized) {
                exit_code = EXIT_FAILURE;
            }
        }

        if (exit_code == EXIT_SUCCESS) {
            fr::AssetRegistry registry(alloc);
            fr::AssetStorage storage(alloc);

            if (!cook_default_renderer_shaders(alloc, registry)) {
                FR_LOG_ERR("[DevToolsDemo] Failed to prepare default renderer shaders.");
                exit_code = EXIT_FAILURE;
            }

            if (exit_code == EXIT_SUCCESS) {
                if (!load_dev_asset_manifest_if_exists(alloc, registry, storage,
                                                       "assets/dev.fmanifest")) {
                    exit_code = EXIT_FAILURE;
                }
            }

            if (exit_code == EXIT_SUCCESS) {
                fr::AssetManager assets(device, alloc, &registry, &storage);

                fr::DefaultRendererShaderIds shader_ids{};
                fr::DefaultRendererShaders shaders{};
                bool shaders_loaded = false;

                if (!fr::load_default_renderer_shaders(assets, shader_ids, shaders)) {
                    FR_LOG_ERR("[DevToolsDemo] Failed to load default renderer shaders.");
                    exit_code = EXIT_FAILURE;
                } else {
                    shaders_loaded = true;
                }

                if (exit_code == EXIT_SUCCESS) {
                    fr::RenderPipelineCache pipeline_cache(device, &assets, alloc);

                    fr::RendererPipelineSet pipelines{};
                    if (!fr::create_default_renderer_pipelines(pipeline_cache, shaders,
                                                               pipelines)) {
                        FR_LOG_ERR("[DevToolsDemo] Failed to create renderer pipelines.");
                        exit_code = EXIT_FAILURE;
                    }

                    if (exit_code == EXIT_SUCCESS) {
                        fr::RendererCreateDesc renderer_desc{};
                        renderer_desc.alloc = alloc;
                        renderer_desc.pipelines = pipelines;

                        fr::Renderer renderer(device, renderer_desc);
                        if (!renderer.is_ready()) {
                            FR_LOG_ERR("[DevToolsDemo] Renderer failed to initialize.");
                            exit_code = EXIT_FAILURE;
                        }

                        if (exit_code == EXIT_SUCCESS) {
                            fr::World world{};

                            fr::devtools::DevToolsState tools{};
                            fr::devtools::SpawnPanelState spawn_panel_state{};
                            fr::asscooker::imgui::GltfImportPanelState gltf_panel_state{};
                            fr::asscooker::DevAssetCatalog asset_catalog(alloc);

                            init_devtools_defaults(tools);

                            DevToolsRuntimeResource runtime_resource{};
                            runtime_resource.alloc = alloc;
                            runtime_resource.registry = &registry;
                            runtime_resource.storage = &storage;
                            runtime_resource.assets = &assets;
                            runtime_resource.asset_catalog = &asset_catalog;
                            runtime_resource.tools = &tools;
                            runtime_resource.spawn_panel = &spawn_panel_state;
                            runtime_resource.gltf_import_panel = &gltf_panel_state;

                            fr::WindowInput input{};

                            DemoFrameResource frame_resource{};
                            frame_resource.window = &window;
                            frame_resource.input = &input;

                            world.emplace_resource<DevToolsRuntimeResource>(runtime_resource);
                            world.emplace_resource<DemoFrameResource>(frame_resource);

                            fr::devtools::EditorContext setup_ctx{};
                            setup_ctx.world = &world;
                            setup_ctx.assets = &assets;
                            setup_ctx.tools = &tools;

                            setup_default_scene(setup_ctx);

                            world.schedule(fr::Stage::Update, camera_control_system);
                            world.schedule(fr::Stage::Update, devtools_ui_system);

                            fr::RenderFrameSubmission submission(alloc);

                            window.set_mouse_mode(fr::MouseMode::Normal);

                            using Clock = std::chrono::steady_clock;
                            Clock::time_point last_frame_time = Clock::now();

                            bool running = true;

                            while (running) {
                                const Clock::time_point now = Clock::now();
                                const std::chrono::duration<F32> frame_delta =
                                    now - last_frame_time;
                                last_frame_time = now;

                                F32 dt = frame_delta.count();
                                if (dt > 0.1f) {
                                    dt = 0.1f;
                                }

                                if (!window.poll_events(input) ||
                                    input.is_key_pressed(fr::Key::Escape)) {
                                    running = false;
                                    break;
                                }

                                DemoFrameResource &frame = world.get_resource<DemoFrameResource>();
                                frame.dt = dt;

                                ImGui_ImplOpenGL3_NewFrame();
                                ImGui_ImplSDL3_NewFrame();
                                ImGui::NewFrame();

                                world.run();
                                world.commit();

                                fr::TransformSystem::rebuild_world_transforms(world);
                                fr::RenderAssetSystem::resolve(world, assets);
                                assets.process_async_uploads(ASYNC_UPLOAD_BUDGET_PER_FRAME);

                                if (!window.is_minimized() && window.get_width() > 0 &&
                                    window.get_height() > 0) {
                                    const U32 width = window.get_width();
                                    const U32 height = window.get_height();

                                    const F32 aspect =
                                        static_cast<F32>(width) / static_cast<F32>(height);

                                    fr::RenderExtractDesc extract_desc{};
                                    extract_desc.aspect_ratio = aspect;
                                    extract_desc.geometry_pipeline =
                                        renderer.geometry_pipeline(false);
                                    extract_desc.forward_transparent_pipeline =
                                        renderer.forward_transparent_pipeline();
                                    extract_desc.shadow_pipeline = renderer.shadow_pipeline();

                                    fr::RenderExtractResult extract_result =
                                        fr::extract_render_frame(world, assets, extract_desc,
                                                                 submission);

                                    fr::RenderFrameDesc frame_desc = build_frame_desc(
                                        extract_result, submission, width, height, tools);

                                    renderer.render(frame_desc);
                                }

                                ImGui::Render();
                                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                                window.swap_buffers();
                            }

                            window.set_mouse_mode(fr::MouseMode::Normal);

                            fr::RenderAssetSystem::release(world, assets);
                        }
                    }
                }

                if (shaders_loaded) {
                    fr::unload_default_renderer_shaders(assets, shaders);
                }
            }
        }

        if (imgui_initialized) {
            shutdown_imgui(window);
        }

        if (device) {
            fr::destroy_opengl_render_device(device);
            device = nullptr;
        }

        window.close();

        if (exit_code == EXIT_SUCCESS) {
            FR_LOG_OK("[DevToolsDemo] Shutdown complete.");
        } else {
            FR_LOG_ERR("[DevToolsDemo] Shutdown after failure.");
        }
    }

    fr::shutdown_core_ctx();
    return exit_code;
}
