/**
 * @file inspector.cpp
 * @author Tfoedy
 * made by AI to be replaced by good code by humans
 */

#include <SDL3/SDL.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <glm/gtc/quaternion.hpp>

#include "fr/asscooker/asscooker.hpp"
#include "fr/asscooker/dev_asset_catalog.hpp"
#include "fr/asscooker/imgui/gltf_import_panel.hpp"
#include "fr/asscooker/imgui/material_override_panel.hpp"

#include "fr/asset/asset_manager.hpp"
#include "fr/asset/asset_manifest.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/asset/asset_storage.hpp"
#include "fr/asset/material_format.hpp"

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/core/time.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"

#include "fr/devtools/devtools_state.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/devtools/imgui/primitive_panel.hpp"
#include "fr/devtools/imgui/render_settings_panel.hpp"
#include "fr/devtools/imgui/spawn_panel.hpp"
#include "fr/devtools/imgui/stats_panel.hpp"
#include "fr/devtools/imgui/transform_gizmo.hpp"
#include "fr/devtools/inspector.hpp"
#include "fr/devtools/object_picking.hpp"
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

#include "fr/scene/environment_parts.hpp"
#include "fr/scene/environment_system.hpp"
#include "fr/scene/primitive_mesh_system.hpp"
#include "fr/scene/render_asset_system.hpp"
#include "fr/scene/render_extractor.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/scene_render_settings.hpp"
#include "fr/scene/transform_system.hpp"

namespace {

constexpr S32 DEMO_EXIT_SUCCESS = 0;
constexpr S32 DEMO_EXIT_FAILURE = 1;

constexpr USize ASYNC_UPLOAD_BUDGET_PER_FRAME = 8;
constexpr F32 CAMERA_PRECISION_SPEED_SCALE = 0.15f;

constexpr fr::MouseButton CAMERA_MOUSE_BUTTON = static_cast<fr::MouseButton>(3);
constexpr fr::MouseButton PICK_MOUSE_BUTTON = fr::MouseButton::Left;

enum class DemoShadingOverride : S32 {
    MaterialDefault = 0,
    Unlit = 1,
    Standard = 2,
    PBR = 3,
};

struct DefaultShaderCookInput {
    fr::AssetId id{};
    fr::StringView vertex_path{};
    fr::StringView fragment_path{};
    fr::StringView output_path{};
};

struct DemoRendererDebugState {
    S32 shading_override{static_cast<S32>(DemoShadingOverride::MaterialDefault)};

    template <typename Archive>
    void shape(Archive &) noexcept {
        // Runtime-only debug UI state. Intentionally not serialized.
    }
};

struct DemoEnvironmentState {
    char source_hdr_path[512]{};
    char output_ftex_path[512]{"assets/environments/environment.ftex"};
    bool force{true};

    template <typename Archive>
    void shape(Archive &) noexcept {
        // Runtime-only UI state. Intentionally not serialized.
    }
};

struct DevToolsRuntimeResource {
    fr::Alloc *alloc{nullptr};

    fr::AssetRegistry *registry{nullptr};
    fr::AssetStorage *storage{nullptr};
    fr::AssetManager *assets{nullptr};
    fr::asscooker::DevAssetCatalog *asset_catalog{nullptr};

    fr::devtools::DevToolsState *tools{nullptr};
    fr::devtools::SpawnPanelState *spawn_panel{nullptr};
    fr::devtools::PrimitivePanelState *primitive_panel{nullptr};
    fr::devtools::TransformGizmoState *gizmo{nullptr};

    fr::asscooker::imgui::GltfImportPanelState *gltf_import_panel{nullptr};
    fr::asscooker::imgui::MaterialOverridePanelState *material_override_panel{nullptr};

    DemoRendererDebugState *renderer_debug{nullptr};
    DemoEnvironmentState *environment{nullptr};

    template <typename Archive>
    void shape(Archive &) noexcept {
        // Runtime-only non-owning pointers. Intentionally not serialized.
    }
};

struct DemoFrameResource {
    fr::Window *window{nullptr};
    fr::WindowInput *input{nullptr};

    F32 dt{0.0f};
    bool camera_active{false};

    fr::devtools::DevToolsFrameStats stats{};

    template <typename Archive>
    void shape(Archive &) noexcept {
        // Runtime-only frame state. Intentionally not serialized.
    }
};

} // namespace

FR_TYPE(DemoRendererDebugState);
FR_TYPE(DemoEnvironmentState);
FR_TYPE(DevToolsRuntimeResource);
FR_TYPE(DemoFrameResource);

namespace {

void setup_logger() {
    fr::get_ambient_ctx().logger->add_sink(
        fr::make_unique<fr::StandardSink>(fr::StandardSink::Options{}));
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

fr::SceneRenderSettingsPart *find_scene_render_settings(fr::World &world) noexcept {
    fr::SceneRenderSettingsPart *result = nullptr;

    world.for_each_alive_thing([&](fr::Thing thing) noexcept {
        if (result || thing.is_nil()) {
            return;
        }

        result = world.try_get<fr::SceneRenderSettingsPart>(thing);
    });

    return result;
}

fr::SceneRenderSettingsPart &
ensure_scene_render_settings(fr::World &world, const fr::devtools::DevToolsState &tools) noexcept {
    if (fr::SceneRenderSettingsPart *existing = find_scene_render_settings(world)) {
        return *existing;
    }

    fr::Thing thing = world.spawn();
    fr::SceneRenderSettingsPart &settings = world.emplace_now<fr::SceneRenderSettingsPart>(thing);

    settings.lighting = tools.lighting;
    settings.ao = tools.ao;
    settings.ibl = tools.ibl;
    settings.debug = tools.debug;
    settings.directional_shadow_settings = tools.directional_shadow_settings;

    return settings;
}

void copy_scene_settings_to_tools(const fr::SceneRenderSettingsPart &settings,
                                  fr::devtools::DevToolsState &tools) noexcept {
    tools.lighting = settings.lighting;
    tools.ao = settings.ao;
    tools.ibl = settings.ibl;
    tools.debug = settings.debug;
    tools.directional_shadow_settings = settings.directional_shadow_settings;
}

void copy_tools_to_scene_settings(const fr::devtools::DevToolsState &tools,
                                  fr::SceneRenderSettingsPart &settings) noexcept {
    settings.lighting = tools.lighting;
    settings.ao = tools.ao;
    settings.ibl = tools.ibl;
    settings.debug = tools.debug;
    settings.directional_shadow_settings = tools.directional_shadow_settings;
}

fr::EnvironmentPart *find_environment_part(fr::World &world) noexcept {
    fr::EnvironmentPart *result = nullptr;

    world.for_each_alive_thing([&](fr::Thing thing) noexcept {
        if (result || thing.is_nil()) {
            return;
        }

        result = world.try_get<fr::EnvironmentPart>(thing);
    });

    return result;
}

fr::EnvironmentPart &ensure_environment_part(fr::World &world) noexcept {
    if (fr::EnvironmentPart *existing = find_environment_part(world)) {
        return *existing;
    }

    fr::Thing thing = world.spawn();
    return world.emplace_now<fr::EnvironmentPart>(thing);
}

void process_scene_io_requests(fr::World &world, fr::AssetManager &assets,
                               fr::devtools::DevToolsState &tools,
                               fr::devtools::SpawnPanelState &panel) noexcept {
    if (panel.request_save_scene) {
        panel.request_save_scene = false;

        if (panel.scene_path[0] == '\0') {
            FR_LOG_ERR("[DevToolsDemo] Cannot save scene to an empty path.");
        } else {
            fr::devtools::save_scene(world, fr::StringView(panel.scene_path));
        }
    }

    if (panel.request_load_scene) {
        panel.request_load_scene = false;

        if (panel.scene_path[0] == '\0') {
            FR_LOG_ERR("[DevToolsDemo] Cannot load scene from an empty path.");
            return;
        }

        if (fr::devtools::load_scene_replacing_world(world, assets,
                                                     fr::StringView(panel.scene_path))) {
            fr::devtools::set_selected_thing(tools, fr::Thing::nil());
        }
    }
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

    ensure_scene_render_settings(*ctx.world, *ctx.tools);
    ensure_environment_part(*ctx.world);

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

void process_object_picking(fr::World &world, fr::Window &window, fr::WindowInput &input,
                            fr::AssetManager &assets, fr::devtools::DevToolsState &tools,
                            bool camera_active) noexcept {
    if (camera_active || !window.is_focused()) {
        return;
    }

    if (window.is_minimized() || window.get_width() == 0 || window.get_height() == 0) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();

    if (io.WantCaptureMouse || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
        ImGui::IsAnyItemHovered()) {
        return;
    }

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
        return;
    }

    if (!input.is_mouse_pressed(PICK_MOUSE_BUTTON)) {
        return;
    }

    const F32 width = static_cast<F32>(window.get_width());
    const F32 height = static_cast<F32>(window.get_height());
    const F32 aspect = width / height;

    fr::devtools::EditorCameraMatrices camera =
        fr::devtools::extract_editor_camera_matrices(world, aspect);

    if (!camera.found) {
        return;
    }

    fr::devtools::PickingRay ray =
        fr::devtools::make_picking_ray(input.mouse_x, input.mouse_y, width, height, camera);

    fr::devtools::PickingHit hit = fr::devtools::pick_scene_mesh_aabbs(world, assets, ray);

    fr::devtools::set_selected_thing(tools, hit.is_valid() ? hit.thing : fr::Thing::nil());
}

const char *shading_override_name(DemoShadingOverride mode) noexcept {
    switch (mode) {
    case DemoShadingOverride::MaterialDefault:
        return "Material Default";
    case DemoShadingOverride::Unlit:
        return "Force Unlit";
    case DemoShadingOverride::Standard:
        return "Force Standard";
    case DemoShadingOverride::PBR:
        return "Force PBR";
    default:
        return "Unknown";
    }
}

void draw_shading_override_combo(DemoRendererDebugState &state) noexcept {
    ImGui::PushID("demo_shading_override");

    S32 selected = state.shading_override;

    const char *items[] = {
        "Material Default",
        "Force Unlit",
        "Force Standard",
        "Force PBR",
    };

    if (ImGui::Combo("Shading Override##combo", &selected, items, 4)) {
        state.shading_override = glm::clamp(selected, 0, 3);
    }

    ImGui::TextDisabled(
        "%s", shading_override_name(static_cast<DemoShadingOverride>(state.shading_override)));

    ImGui::PopID();
}

void apply_shading_override(fr::RenderFrameSubmission &submission,
                            DemoShadingOverride override_mode) noexcept {
    if (override_mode == DemoShadingOverride::MaterialDefault) {
        return;
    }

    U32 shading_model = static_cast<U32>(fr::MaterialShadingModel::PBR);

    if (override_mode == DemoShadingOverride::Unlit) {
        shading_model = static_cast<U32>(fr::MaterialShadingModel::Unlit);
    } else if (override_mode == DemoShadingOverride::Standard) {
        shading_model = static_cast<U32>(fr::MaterialShadingModel::Standard);
    }

    for (USize i = 0; i < submission.materials.size(); ++i) {
        submission.materials[i].shading_model = shading_model;
    }
}

bool cook_and_apply_hdr_environment(fr::World &world, DevToolsRuntimeResource &runtime,
                                    DemoEnvironmentState &state) noexcept {
    FR_ASSERT(runtime.alloc, "allocator must be available");
    FR_ASSERT(runtime.registry, "AssetRegistry must be available");
    FR_ASSERT(runtime.assets, "AssetManager must be available");
    FR_ASSERT(runtime.asset_catalog, "DevAssetCatalog must be available");

    if (state.source_hdr_path[0] == '\0' || state.output_ftex_path[0] == '\0') {
        FR_LOG_ERR("[Environment] HDR source/output path is empty.");
        return false;
    }

    fr::String source_path =
        fr::String::from_view(runtime.alloc, fr::StringView(state.source_hdr_path));

    if (!fr::file::exists(source_path)) {
        FR_LOG_ERR("[Environment] HDR source file does not exist: {}", source_path.view());
        return false;
    }

    fr::DynamicArray<fr::asscooker::CookedAssetOutput> outputs(runtime.alloc);

    fr::asscooker::CookOptions options{};
    options.force = state.force;
    options.output_id = fr::AssetId::from_logical_path(fr::StringView(state.output_ftex_path));

    if (!fr::asscooker::cook_texture_ex(fr::StringView(state.source_hdr_path),
                                        fr::StringView(state.output_ftex_path), false, &outputs,
                                        options)) {
        FR_LOG_ERR("[Environment] Failed to cook HDR environment: {}",
                   fr::StringView(state.source_hdr_path));
        return false;
    }

    if (!register_cooked_outputs(*runtime.registry, outputs.slice())) {
        FR_LOG_ERR("[Environment] Failed to register cooked HDR environment output.");
        return false;
    }

    runtime.asset_catalog->add_or_replace(outputs.slice(), fr::StringView(state.source_hdr_path));

    if (!runtime.asset_catalog->build_loose_manifest(fr::StringView("assets/dev.fmanifest"))) {
        FR_LOG_ERR("[Environment] Failed to rebuild development asset manifest.");
        return false;
    }

    fr::EnvironmentPart &env = ensure_environment_part(world);

    if (env.texture_handle.is_valid()) {
        runtime.assets->unload_texture(env.texture_handle);
    }

    env.texture_path = fr::String::from_view(fr::StringView(state.output_ftex_path));
    env.texture_id = fr::AssetId::from_logical_path(env.texture_path.view());
    env.resolved_texture_id = {};
    env.texture_handle = {};
    env.enabled = true;

    if (runtime.tools) {
        fr::SceneRenderSettingsPart &settings = ensure_scene_render_settings(world, *runtime.tools);
        settings.ibl.enabled = true;
        copy_scene_settings_to_tools(settings, *runtime.tools);
    }

    fr::EnvironmentSystem::resolve(world, *runtime.assets);

    FR_LOG_OK("[Environment] Imported HDR environment: {}", fr::StringView(state.output_ftex_path));
    return true;
}

void draw_environment_panel(fr::World &world, DevToolsRuntimeResource &runtime) noexcept {
    FR_ASSERT(runtime.environment, "Environment state must be available");

    DemoEnvironmentState &state = *runtime.environment;
    fr::EnvironmentPart &env = ensure_environment_part(world);

    ImGui::PushID("environment_panel");

    ImGui::InputText("Source HDR##source_hdr", state.source_hdr_path,
                     sizeof(state.source_hdr_path));
    ImGui::InputText("Output .ftex##output_ftex", state.output_ftex_path,
                     sizeof(state.output_ftex_path));

    ImGui::Checkbox("Force Recook##force_hdr", &state.force);
    ImGui::Separator();

    ImGui::Checkbox("Scene Environment Enabled##scene_env_enabled", &env.enabled);

    if (env.texture_path.size() != 0) {
        ImGui::TextWrapped("Scene environment: %s", env.texture_path.c_str());
    } else {
        ImGui::TextDisabled("Scene has no environment texture.");
    }

    ImGui::Text("Handle: %s", env.texture_handle.is_valid() ? "valid" : "invalid");

    if (ImGui::Button("Import / Apply HDR##import_hdr")) {
        cook_and_apply_hdr_environment(world, runtime, state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Resolve Scene Environment##resolve_scene_env")) {
        fr::EnvironmentSystem::resolve(world, *runtime.assets);
    }

    ImGui::PopID();
}

static void devtools_ui_system(fr::Scope scope) {
    DevToolsRuntimeResource &runtime = scope.get_resource<DevToolsRuntimeResource>();

    FR_ASSERT(runtime.assets, "AssetManager must be available");
    FR_ASSERT(runtime.registry, "AssetRegistry must be available");
    FR_ASSERT(runtime.asset_catalog, "DevAssetCatalog must be available");
    FR_ASSERT(runtime.tools, "DevToolsState must be available");
    FR_ASSERT(runtime.spawn_panel, "SpawnPanelState must be available");
    FR_ASSERT(runtime.primitive_panel, "PrimitivePanelState must be available");
    FR_ASSERT(runtime.gizmo, "TransformGizmoState must be available");
    FR_ASSERT(runtime.gltf_import_panel, "GltfImportPanelState must be available");
    FR_ASSERT(runtime.material_override_panel, "MaterialOverridePanelState must be available");
    FR_ASSERT(runtime.renderer_debug, "DemoRendererDebugState must be available");
    FR_ASSERT(runtime.environment, "DemoEnvironmentState must be available");

    fr::devtools::EditorContext editor_ctx{};
    editor_ctx.world = &scope.world();
    editor_ctx.assets = runtime.assets;
    editor_ctx.tools = runtime.tools;

    if (runtime.tools->show_inspector) {
        ImGui::SetNextWindowSize(ImVec2(1100.0f, 660.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);

        ImGui::PushID("world_inspector_window");
        if (ImGui::Begin("World Inspector##main_world_inspector", &runtime.tools->show_inspector)) {
            fr::devtools::inspector(editor_ctx);
        }
        ImGui::End();
        ImGui::PopID();
    }

    if (runtime.tools->show_spawn_panel) {
        ImGui::SetNextWindowSize(ImVec2(460.0f, 720.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(1140.0f, 20.0f), ImGuiCond_FirstUseEver);

        ImGui::PushID("spawn_scene_window");
        if (ImGui::Begin("Spawn / Scene##main_spawn_scene", &runtime.tools->show_spawn_panel)) {
            ImGui::PushID("spawn_panel");
            fr::devtools::spawn_panel(editor_ctx, *runtime.spawn_panel);
            ImGui::PopID();

            ImGui::SeparatorText("Primitives##primitive_separator");

            ImGui::PushID("primitive_spawn");
            fr::devtools::primitive_spawn_panel(editor_ctx, *runtime.primitive_panel,
                                                runtime.spawn_panel->transform);
            ImGui::PopID();

            ImGui::SeparatorText("Selected Primitive##selected_primitive_separator");

            ImGui::PushID("selected_primitive");
            fr::devtools::selected_primitive_panel(editor_ctx);
            ImGui::PopID();
        }
        ImGui::End();
        ImGui::PopID();
    }

    if (runtime.tools->show_render_settings) {
        DemoFrameResource &frame = scope.get_resource<DemoFrameResource>();

        ImGui::SetNextWindowSize(ImVec2(480.0f, 720.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 700.0f), ImGuiCond_FirstUseEver);

        ImGui::PushID("renderer_stats_window");
        if (ImGui::Begin("Renderer / Stats##main_renderer_stats",
                         &runtime.tools->show_render_settings)) {
            if (ImGui::BeginTabBar("##renderer_stats_tabs")) {
                if (ImGui::BeginTabItem("Renderer##renderer_settings_tab")) {
                    fr::SceneRenderSettingsPart &scene_settings =
                        ensure_scene_render_settings(scope.world(), *runtime.tools);

                    copy_scene_settings_to_tools(scene_settings, *runtime.tools);

                    ImGui::PushID("render_settings_panel");
                    fr::devtools::render_settings_panel(*runtime.tools);
                    ImGui::PopID();

                    copy_tools_to_scene_settings(*runtime.tools, scene_settings);

                    ImGui::SeparatorText("Material Debug##material_debug_separator");
                    draw_shading_override_combo(*runtime.renderer_debug);

                    ImGui::SeparatorText("Transform Gizmo##transform_gizmo_separator");
                    fr::devtools::draw_transform_gizmo_toolbar(*runtime.gizmo);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Stats##stats_tab")) {
                    ImGui::PushID("stats_panel");
                    fr::devtools::stats_panel(scope.world(), frame.stats);
                    ImGui::PopID();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        ImGui::PopID();
    }

    ImGui::SetNextWindowSize(ImVec2(540.0f, 700.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(1140.0f, 760.0f), ImGuiCond_FirstUseEver);

    ImGui::PushID("assets_materials_window");
    if (ImGui::Begin("Assets / Materials##main_assets_materials")) {
        fr::asscooker::DevAssetImportContext import_ctx{};
        import_ctx.alloc = runtime.alloc;
        import_ctx.registry = runtime.registry;
        import_ctx.catalog = runtime.asset_catalog;
        import_ctx.cooked_root = "assets";
        import_ctx.manifest_path = "assets/dev.fmanifest";

        if (ImGui::BeginTabBar("##assets_materials_tabs")) {
            if (ImGui::BeginTabItem("glTF##gltf_tab")) {
                ImGui::PushID("gltf_import_panel");
                [[maybe_unused]] fr::asscooker::ImportedModelResult imported =
                    fr::asscooker::imgui::draw_gltf_import_panel(import_ctx, editor_ctx,
                                                                 *runtime.gltf_import_panel);
                ImGui::PopID();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Material Override##material_override_tab")) {
                ImGui::PushID("material_override_panel");
                fr::asscooker::imgui::material_override_panel(import_ctx, editor_ctx,
                                                              *runtime.material_override_panel);
                ImGui::PopID();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Environment HDR##environment_hdr_tab")) {
                draw_environment_panel(scope.world(), runtime);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopID();
}

fr::RenderFrameDesc build_frame_desc(fr::World &world,
                                     const fr::RenderExtractResult &extract_result,
                                     const fr::RenderFrameSubmission &submission, U32 width,
                                     U32 height, const fr::devtools::DevToolsState &tools,
                                     const fr::AssetManager &assets) noexcept {
    fr::RenderFrameDesc frame{};
    frame.submission = &submission;
    frame.viewport.width = width;
    frame.viewport.height = height;
    frame.camera = extract_result.camera;
    frame.environment_source = fr::EnvironmentSystem::active_environment_texture(world, assets);
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

    S32 exit_code = DEMO_EXIT_SUCCESS;

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
            exit_code = DEMO_EXIT_FAILURE;
        }

        if (exit_code == DEMO_EXIT_SUCCESS) {
            device = fr::create_opengl_render_device(alloc);
            if (!device) {
                FR_LOG_ERR("[DevToolsDemo] Failed to create OpenGL render device.");
                exit_code = DEMO_EXIT_FAILURE;
            }
        }

        if (exit_code == DEMO_EXIT_SUCCESS) {
            imgui_initialized = init_imgui(window);
            if (!imgui_initialized) {
                exit_code = DEMO_EXIT_FAILURE;
            }
        }

        if (exit_code == DEMO_EXIT_SUCCESS) {
            fr::AssetRegistry registry(alloc);
            fr::AssetStorage storage(alloc);

            if (!cook_default_renderer_shaders(alloc, registry)) {
                FR_LOG_ERR("[DevToolsDemo] Failed to prepare default renderer shaders.");
                exit_code = DEMO_EXIT_FAILURE;
            }

            if (exit_code == DEMO_EXIT_SUCCESS) {
                if (!load_dev_asset_manifest_if_exists(alloc, registry, storage,
                                                       "assets/dev.fmanifest")) {
                    exit_code = DEMO_EXIT_FAILURE;
                }
            }

            if (exit_code == DEMO_EXIT_SUCCESS) {
                fr::AssetManager assets(device, alloc, &registry, &storage);

                fr::DefaultRendererShaderIds shader_ids{};
                fr::DefaultRendererShaders shaders{};
                bool shaders_loaded = false;

                if (!fr::load_default_renderer_shaders(assets, shader_ids, shaders)) {
                    FR_LOG_ERR("[DevToolsDemo] Failed to load default renderer shaders.");
                    exit_code = DEMO_EXIT_FAILURE;
                } else {
                    shaders_loaded = true;
                }

                if (exit_code == DEMO_EXIT_SUCCESS) {
                    fr::RenderPipelineCache pipeline_cache(device, &assets, alloc);

                    fr::RendererPipelineSet pipelines{};
                    if (!fr::create_default_renderer_pipelines(pipeline_cache, shaders,
                                                               pipelines)) {
                        FR_LOG_ERR("[DevToolsDemo] Failed to create renderer pipelines.");
                        exit_code = DEMO_EXIT_FAILURE;
                    }

                    if (exit_code == DEMO_EXIT_SUCCESS) {
                        fr::RendererCreateDesc renderer_desc{};
                        renderer_desc.alloc = alloc;
                        renderer_desc.pipelines = pipelines;

                        fr::Renderer renderer(device, renderer_desc);
                        if (!renderer.is_ready()) {
                            FR_LOG_ERR("[DevToolsDemo] Renderer failed to initialize.");
                            exit_code = DEMO_EXIT_FAILURE;
                        }

                        if (exit_code == DEMO_EXIT_SUCCESS) {
                            fr::World world{};

                            fr::devtools::DevToolsState tools{};
                            fr::devtools::SpawnPanelState spawn_panel_state{};
                            fr::devtools::PrimitivePanelState primitive_panel_state{};
                            fr::devtools::TransformGizmoState gizmo_state{};

                            fr::asscooker::imgui::GltfImportPanelState gltf_panel_state{};
                            fr::asscooker::imgui::MaterialOverridePanelState material_panel_state{};
                            fr::asscooker::DevAssetCatalog asset_catalog(alloc);

                            DemoRendererDebugState renderer_debug_state{};
                            DemoEnvironmentState environment_state{};

                            init_devtools_defaults(tools);

                            DevToolsRuntimeResource runtime_resource{};
                            runtime_resource.alloc = alloc;
                            runtime_resource.registry = &registry;
                            runtime_resource.storage = &storage;
                            runtime_resource.assets = &assets;
                            runtime_resource.asset_catalog = &asset_catalog;
                            runtime_resource.tools = &tools;
                            runtime_resource.spawn_panel = &spawn_panel_state;
                            runtime_resource.primitive_panel = &primitive_panel_state;
                            runtime_resource.gizmo = &gizmo_state;
                            runtime_resource.gltf_import_panel = &gltf_panel_state;
                            runtime_resource.material_override_panel = &material_panel_state;
                            runtime_resource.renderer_debug = &renderer_debug_state;
                            runtime_resource.environment = &environment_state;

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

                            auto last_frame_time = fr::time::get_steady_now_ms();
                            bool running = true;

                            while (running) {
                                const auto now = fr::time::get_steady_now_ms();
                                F32 dt = static_cast<F32>(now - last_frame_time) * 0.001f;
                                last_frame_time = now;

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
                                frame.stats.dt = dt;
                                frame.stats.fps = dt > 0.0f ? 1.0f / dt : 0.0f;
                                frame.stats.viewport_width = window.get_width();
                                frame.stats.viewport_height = window.get_height();

                                ImGui_ImplOpenGL3_NewFrame();
                                ImGui_ImplSDL3_NewFrame();
                                ImGui::NewFrame();

                                fr::devtools::update_transform_gizmo_shortcuts(gizmo_state);

                                world.run();
                                world.commit();

                                process_scene_io_requests(world, assets, tools, spawn_panel_state);

                                fr::TransformSystem::rebuild_world_transforms(world);
                                fr::PrimitiveMeshSystem::resolve(world, assets, alloc);
                                fr::RenderAssetSystem::resolve(world, assets);
                                fr::EnvironmentSystem::resolve(world, assets);
                                assets.process_async_uploads(ASYNC_UPLOAD_BUDGET_PER_FRAME);

                                if (!window.is_minimized() && window.get_width() > 0 &&
                                    window.get_height() > 0) {
                                    const U32 width = window.get_width();
                                    const U32 height = window.get_height();

                                    const F32 aspect =
                                        static_cast<F32>(width) / static_cast<F32>(height);

                                    fr::SceneRenderSettingsPart &scene_settings =
                                        ensure_scene_render_settings(world, tools);

                                    copy_scene_settings_to_tools(scene_settings, tools);

                                    fr::RenderExtractDesc extract_desc{};
                                    extract_desc.aspect_ratio = aspect;
                                    extract_desc.geometry_pipeline =
                                        renderer.geometry_pipeline(false);
                                    extract_desc.forward_transparent_pipeline =
                                        renderer.forward_transparent_pipeline();
                                    extract_desc.shadow_pipeline = renderer.shadow_pipeline();
                                    extract_desc.directional_shadow_settings =
                                        tools.directional_shadow_settings;

                                    fr::RenderExtractResult extract_result =
                                        fr::extract_render_frame(world, assets, extract_desc,
                                                                 submission);

                                    apply_shading_override(
                                        submission, static_cast<DemoShadingOverride>(
                                                        renderer_debug_state.shading_override));

                                    frame.stats.geometry_stats = extract_result.geometry_stats;
                                    frame.stats.shadow_stats = extract_result.shadow_stats;
                                    frame.stats.has_main_camera = extract_result.has_main_camera;

                                    fr::RenderFrameDesc frame_desc =
                                        build_frame_desc(world, extract_result, submission, width,
                                                         height, tools, assets);

                                    renderer.render(frame_desc);

                                    fr::devtools::draw_transform_gizmo(world, tools, gizmo_state,
                                                                       static_cast<F32>(width),
                                                                       static_cast<F32>(height));

                                    process_object_picking(world, window, input, assets, tools,
                                                           frame.camera_active);
                                }

                                ImGui::Render();
                                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                                window.swap_buffers();
                            }

                            window.set_mouse_mode(fr::MouseMode::Normal);

                            fr::EnvironmentSystem::release(world, assets);
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

        if (exit_code == DEMO_EXIT_SUCCESS) {
            FR_LOG_OK("[DevToolsDemo] Shutdown complete.");
        } else {
            FR_LOG_ERR("[DevToolsDemo] Shutdown after failure.");
        }
    }

    fr::shutdown_core_ctx();
    return exit_code;
}
