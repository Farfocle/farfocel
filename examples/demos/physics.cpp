/**
 * @file physics.cpp
 * @author Kiju
 * @brief Farfocel physics demo.
 */

#include <ImVectorEditor.h>
#include <glm/gtc/quaternion.hpp>

#include "fr/asscooker/dev_asset_catalog.hpp"
#include "fr/asscooker/devtools_setup.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/math.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/setup.hpp"
#include "fr/devtools/state.hpp"
#include "fr/logger/logger.hpp"
#include "fr/logger/sinks/pretty_sink.hpp"
#include "fr/physics/parts.hpp"
#include "fr/physics/resources.hpp"
#include "fr/physics/systems.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/keycode.hpp"
#include "fr/scene/app.hpp"
#include "fr/scene/app_state.hpp"
#include "fr/scene/environment.hpp"
#include "fr/scene/imgui_setup.hpp"
#include "fr/scene/render_assets.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/scene_io.hpp"
#include "fr/scene/transform.hpp"

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

static constexpr F32 LINEAR_DAMPING = 2.5f;
static constexpr F32 ANGULAR_DAMPING = 7.0f;
static constexpr F32 PLAYER_MOVE_FORCE = 60.0f;

static constexpr F32 CAMERA_DIST = 14.0f;
static constexpr F32 CAMERA_HEIGHT = 5.0f;
static constexpr F32 THROW_SPEED = 15.0f;

struct PlayerState {
    fr::Thing player{fr::Thing::nil()};
};

/// @brief Inserts RigidBodyPart, MassPart, and ColliderPart onto `thing` immediately.
/// Local AABB is always Vec3(0.5) — do_aabb_to_world multiplies by the world scale from
/// LocalTransformPart, so scale must not be baked into the local half-extents.
static void ensure_physics_parts(fr::World &world, fr::Thing thing, F32 mass, F32 restitution,
                                 F32 friction, fr::Vec3 half_extents) noexcept {
    world.insert_now<fr::RigidBodyPart>(
        thing, fr::RigidBodyPart::make_dynamic(mass, restitution, friction));
    world.insert_now<fr::MassPart>(thing, fr::MassPart::from_box(mass, half_extents));
    world.insert_now<fr::ColliderPart>(
        thing, fr::ColliderPart::make(fr::AABB::from_center(fr::Vec3{}, fr::Vec3(0.5f))));
}

static void physics_damping_system(fr::Scope scope) {
    const fr::PhysicsState &phys = scope.get_resource<fr::PhysicsState>();
    if (!phys.is_running) {
        return;
    }
    const F32 dt = phys.dt;

    for (auto [thing, rb] : scope.query<fr::RigidBodyPart>()) {
        (void)thing;
        if (rb.inv_mass <= 0.0f) {
            continue;
        }

        rb.velocity *= fr::math::max(0.0f, 1.0f - LINEAR_DAMPING * dt);
        rb.angular_velocity *= fr::math::max(0.0f, 1.0f - ANGULAR_DAMPING * dt);

        // Hard cap so a bad collision impulse can't blow up the simulation.
        const F32 lin_speed = glm::length(rb.velocity);
        if (lin_speed > 20.0f) {
            rb.velocity *= 20.0f / lin_speed;
        }
        const F32 ang_speed = glm::length(rb.angular_velocity);
        if (ang_speed > 15.0f) {
            rb.angular_velocity *= 15.0f / ang_speed;
        }
    }
}

static void player_control_system(fr::Scope scope) {
    auto &input = scope.get_resource<fr::WindowInput>();
    auto &phys = scope.get_resource<fr::PhysicsState>();

    if (input.is_key_pressed(fr::Key::P) && !ImGui::GetIO().WantTextInput) {
        phys.is_running = !phys.is_running;
    }

    if (!phys.is_running) {
        return;
    }

    fr::Vec3 move_forward{0.0f, 0.0f, -1.0f};
    fr::Vec3 move_right{1.0f, 0.0f, 0.0f};

    for (auto [cam_thing, fps, cam_wt] :
         scope.query<fr::FPSControllerPart, fr::WorldTransformPart>()) {
        (void)cam_thing;

        const fr::Vec3 fwd = cam_wt.rotation * fr::Vec3(0.0f, 0.0f, -1.0f);
        const fr::Vec3 rgt = cam_wt.rotation * fr::Vec3(1.0f, 0.0f, 0.0f);

        const F32 fl = glm::length(fr::Vec3(fwd.x, 0.0f, fwd.z));
        const F32 rl = glm::length(fr::Vec3(rgt.x, 0.0f, rgt.z));

        if (fl > 0.001f) {
            move_forward = fr::Vec3(fwd.x, 0.0f, fwd.z) / fl;
        }
        if (rl > 0.001f) {
            move_right = fr::Vec3(rgt.x, 0.0f, rgt.z) / rl;
        }
        break;
    }

    fr::Vec3 move_dir{};
    if (input.is_key_down(fr::Key::W))
        move_dir += move_forward;
    if (input.is_key_down(fr::Key::S))
        move_dir -= move_forward;
    if (input.is_key_down(fr::Key::A))
        move_dir -= move_right;
    if (input.is_key_down(fr::Key::D))
        move_dir += move_right;

    const F32 dir_len = glm::length(move_dir);
    if (dir_len > 0.001f) {
        move_dir /= dir_len;
    }

    const auto &ps = scope.get_resource<PlayerState>();
    if (fr::RigidBodyPart *rb = scope.try_get<fr::RigidBodyPart>(ps.player)) {
        rb->force += move_dir * PLAYER_MOVE_FORCE;
    }

    if (input.is_key_pressed(fr::Key::F) && !ImGui::GetIO().WantTextInput) {
        for (auto [cam_thing, fps, cam_wt] :
             scope.query<fr::FPSControllerPart, fr::WorldTransformPart>()) {
            (void)cam_thing;

            const fr::Vec3 fwd = cam_wt.rotation * fr::Vec3(0.0f, 0.0f, -1.0f);
            const fr::Vec3 spawn_pos = cam_wt.position + fwd * 3.0f;

            fr::LocalTransformPart local{};
            local.position = spawn_pos;

            fr::World &w = scope.world();
            auto &tools = scope.get_resource<fr::devtools::DevToolsState>();
            fr::Thing cube = fr::devtools::spawn_cube(w, tools, local);

            fr::RigidBodyPart cube_rb = fr::RigidBodyPart::make_dynamic(1.0f, 0.2f, 0.5f);
            cube_rb.velocity = fwd * THROW_SPEED;

            w.insert<fr::RigidBodyPart>(cube, std::move(cube_rb));
            w.insert<fr::MassPart>(cube, fr::MassPart::from_box(1.0f, fr::Vec3(0.5f)));
            w.insert<fr::ColliderPart>(
                cube, fr::ColliderPart::make(fr::AABB::from_center(fr::Vec3{}, fr::Vec3(0.5f))));
            break;
        }
    }
}

static void camera_follow_system(fr::Scope scope) {
    const auto &ps = scope.get_resource<PlayerState>();
    const fr::WorldTransformPart *player_wt = scope.try_get<fr::WorldTransformPart>(ps.player);
    if (!player_wt) {
        return;
    }

    for (auto [cam_thing, fps, local] :
         scope.query<fr::FPSControllerPart, fr::LocalTransformPart>()) {
        (void)cam_thing;

        const fr::Quat rot =
            glm::quat(glm::vec3(glm::radians(fps.pitch), glm::radians(fps.yaw), 0.0f));
        const fr::Vec3 fwd = rot * fr::Vec3(0.0f, 0.0f, -1.0f);

        local.position =
            player_wt->position - fwd * CAMERA_DIST + fr::Vec3(0.0f, CAMERA_HEIGHT, 0.0f);

        break;
    }

    fr::rebuild_world_transforms(scope.world());
}

struct Game {
    fr::scene::RendererApp app{};
    fr::asscooker::DevAssetCatalog catalog{};
    fr::World world{};

    bool init() {
        if (!do_init_backend()) {
            return false;
        }
        do_init_physics();
        if (!do_init_scene()) {
            return false;
        }
        return true;
    }

    [[nodiscard]] bool poll() {
        auto &input = world.get_resource<fr::WindowInput>();
        return app.window.poll_events(input) && !input.is_key_pressed(fr::Key::Escape);
    }

    void run() {
        auto &state = world.get_resource<fr::scene::AppState>();
        auto &tools = world.get_resource<fr::devtools::DevToolsState>();

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

        do_draw_hud();

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
                .title = "Farfocel Physics Demo",
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

    void do_init_physics() {
        auto &physics = world.emplace_resource<fr::PhysicsState>();
        physics.options.gravity = fr::Vec3(0.0f, -9.81f, 0.0f);
        physics.is_running = false;

        world.schedule(fr::Stage::PreUpdate, [](fr::Scope scope) {
            const auto &state = scope.get_resource<fr::scene::AppState>();
            auto &phys = scope.get_resource<fr::PhysicsState>();
            phys.dt = state.dt > 0.0f ? state.dt : (1.0f / 60.0f);
        });

        world.schedule(fr::Stage::PreUpdate, fr::rigit_body_force_system);

        world.schedule(fr::Stage::PreUpdate,
                       [](fr::Scope scope) { fr::rebuild_world_transforms(scope.world()); });

        world.schedule(fr::Stage::PreUpdate, fr::broadphase_collision_detection_system);
        world.schedule(fr::Stage::PreUpdate, fr::narrowphase_collision_detection_system);
        world.schedule(fr::Stage::PreUpdate, fr::rigit_body_collision_resolution_system);

        world.schedule(fr::Stage::PreUpdate, physics_damping_system);
        world.schedule(fr::Stage::Update, player_control_system);
        world.schedule(fr::Stage::Update, camera_follow_system);
    }

    bool do_init_scene() {
        auto &tools = world.get_resource<fr::devtools::DevToolsState>();
        fr::scene::ensure_scene_parts(world);

        fr::LocalTransformPart camera_transform{};
        camera_transform.position = fr::Vec3(0.0f, 8.0f, 12.0f);
        camera_transform.rotation = glm::quat(glm::radians(fr::Vec3(-20.0f, 0.0f, 0.0f)));
        fr::Thing camera = fr::devtools::spawn_camera(world, tools, camera_transform);

        fr::LocalTransformPart sun_transform{};
        sun_transform.rotation = glm::quat(glm::radians(fr::Vec3(-55.0f, 40.0f, 0.0f)));
        fr::Thing sun = fr::devtools::spawn_directional_light(world, tools, sun_transform);

        fr::LocalTransformPart ground_transform{};
        ground_transform.position = fr::Vec3(0.0f, -15.0f, 0.0f);
        ground_transform.scale = fr::Vec3(200.0f, 10.0f, 200.0f);
        fr::Thing ground = fr::devtools::spawn_cube(world, tools, ground_transform);

        // Ground position (0,-15,0) scale.y=10  →  top surface at y = -15 + 5 = -10.
        // All spawn positions are computed relative to that surface.
        static constexpr F32 SURF = -10.0f;

        // Player starts 1.5 m above the surface (bottom at -10, falls gently to land).
        fr::LocalTransformPart player_transform{};
        player_transform.position = fr::Vec3(0.0f, SURF + 2.0f, 0.0f);
        fr::Thing player = fr::devtools::spawn_cube(world, tools, player_transform);

        struct CubeDesc {
            fr::Vec3 pos;
            F32 scale;
            F32 mass;
        };

        // Spawn cubes just above the surface so first-contact velocity is tiny.
        // center_y = SURF + scale*0.5 + 0.1  (small gap so they don't start penetrating).
        // Tower at z=7: four cubes stacked 1 m apart, a satisfying target to crash into.
        static const CubeDesc CUBES[] = {
            {{6.0f,  SURF + 0.6f,  4.0f}, 1.0f, 1.0f},
            {{-7.0f, SURF + 0.7f,  3.0f}, 1.2f, 1.5f},   // scale 1.2 → half 0.6
            {{8.0f,  SURF + 0.85f, -5.0f}, 1.5f, 3.0f},  // scale 1.5 → half 0.75
            {{-4.0f, SURF + 0.5f,  -7.0f}, 0.8f, 0.5f},  // scale 0.8 → half 0.4
            // Tower — four stacked cubes (each 1 m apart in Y)
            {{0.0f, SURF + 0.6f, 7.0f}, 1.0f, 1.0f},
            {{0.0f, SURF + 1.6f, 7.0f}, 1.0f, 1.0f},
            {{0.0f, SURF + 2.6f, 7.0f}, 1.0f, 1.0f},
            {{0.0f, SURF + 3.6f, 7.0f}, 1.0f, 1.0f},
        };

        fr::Thing cube_things[std::size(CUBES)];
        for (USize i = 0; i < std::size(CUBES); ++i) {
            fr::LocalTransformPart t{};
            t.position = CUBES[i].pos;
            t.scale = fr::Vec3(CUBES[i].scale);
            cube_things[i] = fr::devtools::spawn_cube(world, tools, t);
        }

        world.commit();

        {
            auto &fps = world.get<fr::FPSControllerPart>(camera);
            fps.pitch = -20.0f;
            fps.yaw = 0.0f;
            fps.move_speed = 15.0f;
            fps.mouse_sensitivity = 0.1f;
        }

        {
            auto &light = world.get<fr::DirectionalLightPart>(sun);
            light.color = fr::Vec3(1.0f, 0.95f, 0.9f);
            light.intensity = 4.0f;
        }

        world.insert_now<fr::RigidBodyPart>(ground, fr::RigidBodyPart::make_static(0.1f, 0.8f));
        world.insert_now<fr::ColliderPart>(
            ground, fr::ColliderPart::make(fr::AABB::from_center(fr::Vec3{}, fr::Vec3(0.5f))));

        ensure_physics_parts(world, player, 2.0f, 0.1f, 0.6f, fr::Vec3(0.5f));

        for (USize i = 0; i < std::size(CUBES); ++i) {
            ensure_physics_parts(world, cube_things[i], CUBES[i].mass, 0.3f, 0.5f,
                                 fr::Vec3(CUBES[i].scale * 0.5f));
        }

        auto &ps = world.emplace_resource<PlayerState>();
        ps.player = player;

        fr::rebuild_world_transforms(world);
        fr::devtools::clear_selection(tools);

        return true;
    }

    void do_draw_hud() {
        const auto &phys = world.get_resource<fr::PhysicsState>();

        ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::Begin("##hud", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::TextUnformatted("WASD        move player");
        ImGui::TextUnformatted("RMB + mouse orbit camera");
        ImGui::TextUnformatted("F           throw cube");
        ImGui::TextUnformatted("P           pause / resume");
        ImGui::TextUnformatted("Escape      quit");
        ImGui::Separator();
        if (phys.is_running) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "RUNNING");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "PAUSED");
        }
        ImGui::End();
    }
};

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
