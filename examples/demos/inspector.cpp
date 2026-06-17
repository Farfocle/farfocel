/**
 * @file inspector.cpp
 * @brief Farfocel devtools inspector demo.
 */

#include <glm/gtc/quaternion.hpp>

#include "fr/asscooker/dev_asset_catalog.hpp"
#include "fr/asscooker/devtools_setup.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/setup.hpp"
#include "fr/devtools/state.hpp"
#include "fr/logger/logger.hpp"
#include "fr/logger/sinks/pretty_sink.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/keycode.hpp"
#include "fr/scene/app.hpp"
#include "fr/scene/environment.hpp"
#include "fr/scene/imgui_setup.hpp"
#include "fr/scene/render_assets.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/scene_io.hpp"

static const fr::asscooker::ShaderCookInput SHADERS[] = {
    {FR_ASSET_ID("renderer.shader.gbuffer"), "engine/shaders/core/gbuffer.vert",
     "engine/shaders/core/gbuffer.frag", "assets/shaders/core/gbuffer.fshader"},

    {FR_ASSET_ID("renderer.shader.forward_transparent"),
     "engine/shaders/core/forward_transparent.vert", "engine/shaders/core/forward_transparent.frag",
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
     "engine/shaders/core/equirect_to_cube.frag", "assets/shaders/core/equirect_to_cube.fshader"},

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

struct Game {
    fr::scene::RendererApp app{};
    fr::asscooker::DevAssetCatalog catalog{};
    fr::World world{};

    bool init() {
        if (!do_init_backend()) {
            return false;
        }

        if (!do_init_scene()) {
            return false;
        }

        return true;
    }

    /// @brief Returns false when the window is closed or Escape is pressed.
    [[nodiscard]] bool poll() {
        auto &input = world.get_resource<fr::WindowInput>();
        return app.window.poll_events(input) && !input.is_key_pressed(fr::Key::Escape);
    }

    void run() {
        auto &state = world.get_resource<fr::scene::AppState>();
        auto &tools = world.get_resource<fr::devtools::DevToolsState>();
        auto &input = world.get_resource<fr::WindowInput>();
        (void)input;

        state.tick();
        tools.dt = state.dt;
        tools.fps = state.fps;
        tools.viewport_width = app.window.get_width();
        tools.viewport_height = app.window.get_height();

        fr::scene::imgui_begin_frame();
        fr::devtools::update_transform_gizmo_shortcuts(tools);

        world.run();
        world.commit();

        fr::devtools::process_scene_io(world);
        app.run(world);

        if (!app.window.is_minimized() && app.window.get_width() > 0 &&
            app.window.get_height() > 0) {
            fr::devtools::draw(world);
        }

        fr::scene::imgui_end_frame();
        app.window.swap_buffers();
    }

    void shutdown() {
        app.window.set_mouse_mode(fr::MouseMode::Normal);
        fr::release_environment(world, *app.assets);
        fr::release_render_assets(world, *app.assets);
        app.shutdown();
    }

private:
    bool do_init_backend() {
        if (!app.init_platform({
                .title = "Farfocel DevTools Demo",
                .width = 1600,
                .height = 900,
                .vsync = true,
            })) {
            return false;
        }

        fr::asscooker::cook_and_register_shaders(
            app.alloc, *app.registry,
            fr::Slice<const fr::asscooker::ShaderCookInput>(SHADERS, std::size(SHADERS)));

        fr::asscooker::load_dev_manifest_if_exists(app.alloc, *app.registry, *app.storage,
                                                   "assets/dev.fmanifest");

        if (!app.init_renderer()) {
            return false;
        }

        fr::devtools::setup_devtools_world(world, app, &catalog);
        app.window.set_mouse_mode(fr::MouseMode::Normal);

        return true;
    }

    bool do_init_scene() {
        auto &tools = world.get_resource<fr::devtools::DevToolsState>();
        fr::scene::ensure_scene_parts(world);

        fr::LocalTransformPart camera_transform{};
        camera_transform.position = fr::Vec3(0.0f, 2.0f, 7.0f);
        camera_transform.rotation = glm::quat(glm::radians(fr::Vec3(-10.0f, 180.0f, 0.0f)));
        fr::Thing camera = fr::devtools::spawn_camera(world, tools, camera_transform);

        fr::LocalTransformPart sun_transform{};
        sun_transform.rotation = glm::quat(glm::radians(fr::Vec3(-60.0f, 30.0f, 0.0f)));
        fr::Thing sun = fr::devtools::spawn_directional_light(world, tools, sun_transform);

        fr::LocalTransformPart point_transform{};
        point_transform.position = fr::Vec3(3.0f, 3.0f, 2.0f);
        fr::Thing point = fr::devtools::spawn_point_light(world, tools, point_transform);

        world.commit();

        {
            fr::FPSControllerPart fps = world.get<fr::FPSControllerPart>(camera);
            (void)fps;
            fps.pitch = -10.0f;
            fps.yaw = 180.0f;
            fps.move_speed = 15.0f;
            fps.mouse_sensitivity = 0.1f;
        }

        {
            fr::DirectionalLightPart light = world.get<fr::DirectionalLightPart>(sun);
            light.color = fr::Vec3(1.0f, 0.95f, 0.9f);
            light.intensity = 4.0f;
        }

        {
            fr::PointLightPart light = world.get<fr::PointLightPart>(point);
            light.color = fr::Vec3(1.0f, 0.82f, 0.6f);
            light.intensity = 10.0f;
            light.radius = 24.0f;
            light.casts_shadow = false;
        }

        fr::rebuild_world_transforms(world);
        fr::devtools::clear_selection(tools);

        return true;
    }
};

// ================================================================= Entry point

S32 main() {
    fr::init_core_ctx();
    fr::get_ambient_ctx().logger->add_sink(
        fr::make_unique<fr::PrettySink>(fr::PrettySink::Options{}));

    {
        Game game{};

        if (game.init()) {
            while (game.poll()) {
                game.run();
            }
        }

        game.shutdown();
    }

    fr::shutdown_core_ctx();
    return 0;
}
