#include "fr/core/ctx.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/time.hpp"
#include "fr/data/asset_manager.hpp"
#include "fr/data/world.hpp"
#include "fr/platform/window.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"
#include "fr/renderer/renderer.hpp"
#include "fr/scene/camera_system.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/render_system.hpp"

#include <fstream>
#include <iostream>

// AI SLOP
fr::String read_shader_file(fr::StringView filepath) {
    fr::String path(filepath);
    std::ifstream file(path.c_str(), std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "[ERROR] Failed to open shader file: " << path << "\n";
        return fr::String();
    }

    USize size = static_cast<USize>(file.tellg());
    file.seekg(0, std::ios::beg);

    fr::String content = fr::String::with_capacity(size);
    content.grow_default(size);
    file.read(content.data(), size);
    return content;
}
// END OF AI SLOP

int main(int argc, char **argv) {
    fr::init_core_ctx();

    fr::WindowProperties props{};
    props.title = "Farfocel Renderer Example (WSAD + MOUSE to move, ECS to quit)";
    props.width = 1600;
    props.height = 900;
    props.vsync = false;
    props.api = fr::GRAPHICS_API::OPENGL;

    fr::Window window;
    if (!window.init(props)) {
        return -1;
    }

    fr::RenderDevice *device = fr::create_opengl_render_device(fr::get_ambient_ctx().alloc);

    {
        fr::AssetManager asset_manager(device, fr::get_ambient_ctx().alloc);
        fr::Renderer renderer(device);
        fr::RenderQueue render_queue(fr::get_ambient_ctx().alloc);
        fr::World world;

        fr::String gbuffer_vert = read_shader_file("engine/shaders/core/gbuffer.vert");
        fr::String gbuffer_frag = read_shader_file("engine/shaders/core/gbuffer.frag");
        fr::ShaderHandle gbuffer_shader =
            device->create_shader(gbuffer_vert.view(), gbuffer_frag.view());

        fr::String lighting_vert = read_shader_file("engine/shaders/core/lighting.vert");
        fr::String lighting_frag = read_shader_file("engine/shaders/core/lighting.frag");
        fr::ShaderHandle lighting_shader =
            device->create_shader(lighting_vert.view(), lighting_frag.view());
        fr::String sh_v = read_shader_file("engine/shaders/core/shadow.vert");
        fr::String sh_f = read_shader_file("engine/shaders/core/shadow.frag");
        fr::ShaderHandle shadow_shader = device->create_shader(sh_v.view(), sh_f.view());

        fr::RenderPipelineProperties shadow_props{};
        shadow_props.shader = shadow_shader;
        shadow_props.depth_test = true;
        shadow_props.depth_write = true;
        shadow_props.cull_mode = fr::CullMode::Front;
        fr::RenderPipelineHandle shadow_pipe = device->create_render_pipeline(shadow_props);
        fr::RenderPipelineProperties gbuffer_props{};
        gbuffer_props.shader = gbuffer_shader;
        gbuffer_props.depth_test = true;
        gbuffer_props.depth_write = true;
        gbuffer_props.cull_mode = fr::CullMode::Back;
        fr::RenderPipelineHandle gbuffer_pipe = device->create_render_pipeline(gbuffer_props);

        fr::RenderPipelineProperties lighting_props{};
        lighting_props.shader = lighting_shader;
        lighting_props.depth_test = false;
        lighting_props.depth_write = false;
        lighting_props.cull_mode = fr::CullMode::None;
        fr::RenderPipelineHandle lighting_pipe = device->create_render_pipeline(lighting_props);

        fr::StringView model_path;
        if (argc > 1) {
            model_path = fr::StringView(argv[1]);
        }

        std::cout << "Loading model: " << fr::String(model_path) << "\n";
        fr::MeshAssetHandle main_mesh = asset_manager.load_mesh(model_path);

        if (main_mesh.is_valid()) {
            fr::Thing main_entity = world.handout();

            auto &trans = world.emplace<fr::TransformPart>(main_entity);
            trans.scale = glm::vec3(1.0f);

            world.emplace<fr::MeshPart>(main_entity, main_mesh);

            auto &mat = world.emplace<fr::MaterialPart>(main_entity);
            mat.shading_model = fr::ShadingModel::PBR;
        }

        fr::Thing sun_entity = world.handout();
        auto &sun_trans = world.emplace<fr::TransformPart>(sun_entity);
        sun_trans.rotation = glm::quat(glm::vec3(glm::radians(-60.0f), glm::radians(30.0f), 0.0f));

        auto &sun_light = world.emplace<fr::DirectionalLightPart>(sun_entity);
        sun_light.color = glm::vec3(1.0f, 0.95f, 0.9f);
        sun_light.intensity = 3.0f;

        fr::Thing point_light_entity = world.handout();
        auto &light_trans = world.emplace<fr::TransformPart>(point_light_entity);
        light_trans.position = glm::vec3(5.0f, 15.0f, 0.0f);

        auto &point_light = world.emplace<fr::PointLightPart>(point_light_entity);
        point_light.color = glm::vec3(1.0f, 0.9f, 0.7f);
        point_light.intensity = 5.0f;
        point_light.radius = 50.0f;

        fr::Thing cam_entity = world.handout();
        world.emplace<fr::CameraPart>(cam_entity);

        auto &cam_trans = world.emplace<fr::TransformPart>(cam_entity);
        cam_trans.position = glm::vec3(0.0f, 5.0f, 0.0f);

        world.emplace<fr::FPSControllerPart>(cam_entity);

        fr::WindowInput input;
        U64 last_time = fr::time::get_steady_now_ns();
        bool running = true;

        while (running) {
            running = window.poll_events(input);
            if (input.is_key_pressed(fr::Key::Escape)) {
                running = false;
            }

            U64 current_time = fr::time::get_steady_now_ms();
            float dt = static_cast<float>(current_time - last_time) / 1000.0f;
            last_time = current_time;

            fr::CameraSystem::update_fps_cameras(world, input, dt);

            render_queue.clear_leftover();
            fr::RenderSystem::submit_meshes(world, render_queue, asset_manager, gbuffer_pipe);
            fr::RenderSystem::submit_lights(world, render_queue);

            fr::RenderSystem::submit_directional_lights(world, render_queue, cam_trans.position);

            render_queue.sort();
            float aspect_ratio =
                static_cast<float>(window.get_width()) / static_cast<float>(window.get_height());
            glm::mat4 view_proj = fr::RenderSystem::extract_cam_view_proj_mtx(world, aspect_ratio);

            renderer.render(render_queue, window.get_width(), window.get_height(), view_proj,
                            cam_trans.position, lighting_pipe, shadow_pipe);
            window.swap_buffers();
        }
    }

    fr::destroy_opengl_render_device(device);
    fr::shutdown_core_ctx();

    return 0;
}
