#include "fr/core/ctx.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/time.hpp"
#include "fr/data/asset_manager.hpp"
#include "fr/data/world.hpp"
#include "fr/platform/window.hpp"
#include "fr/renderer/camera.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"
#include "fr/renderer/renderer.hpp"
#include "fr/scene/components.hpp"
#include "fr/scene/render_system.hpp"

#include <fstream>
#include <iostream>

// we need helpers for handling files asap!
fr::String read_shader_file(fr::StringView filepath) {
    fr::String path(filepath);
    std::ifstream file(path.c_str(), std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "[ERROR] Couldnt load shaders from file: " << path << "\n";
        return fr::String();
    }

    USize size = static_cast<USize>(file.tellg());
    file.seekg(0, std::ios::beg);

    fr::String content = fr::String::with_capacity(size);
    content.grow_default(size);

    file.read(content.data(), size);
    return content;
}

int main(int argc, char **argv) {
    std::cout << "THIS IS TO BE RUN FROM THE TOP FARFOCEL PATH, NOT FROM /bin\n";
    fr::init_core_ctx();

    fr::WindowProperties props{};
    props.title = "Farfocel Renderer Stage 2 - Deferred Shading!!!!!!!!!!!!!!!!!!!!!!!!";
    props.width = 1920;
    props.height = 1080;
    props.vsync = true;
    props.api = fr::GRAPHICS_API::OPENGL;

    fr::Window window;
    if (!window.init(props)) {
        return -1;
    }

    fr::RenderDevice *device = fr::create_opengl_render_device(fr::get_ambient_ctx().alloc);
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
    } else {
        std::cout << "Usage ./bin <path to .gltf file>\n\n";
        return 0;
    }

    std::cout << "Loading model: " << fr::String(model_path) << "\n";
    fr::MeshAssetHandle main_mesh = asset_manager.load_mesh(model_path);

    if (main_mesh.is_valid()) {
        fr::Thing main_entity = world.handout();

        auto &trans = world.emplace<fr::TransformComponent>(main_entity);
        trans.scale = glm::vec3(1.0f);

        world.emplace<fr::MeshComponent>(main_entity, main_mesh);

        auto &mat = world.emplace<fr::MaterialComponent>(main_entity);
        mat.shading_model = fr::ShadingModel::PBR;
    }

    fr::Camera fps_camera(glm::vec3(0.0f, 1.0f, 0.0f), 70.0f, 1280.0f / 720.0f);
    fr::WindowInput input;

    U64 last_time = fr::time::get_steady_now_ms();
    bool running = true;

    while (running) {
        running = window.poll_events(input);
        if (input.is_key_pressed(fr::Key::Escape)) {
            running = false;
        }

        U64 current_time = fr::time::get_steady_now_ms();
        float dt = static_cast<float>(current_time - last_time) / 1000.0f;
        last_time = current_time;

        float speed = 6.7f * dt;
        if (input.is_key_down(fr::Key::W))
            fps_camera.move_forward(speed);
        if (input.is_key_down(fr::Key::S))
            fps_camera.move_forward(-speed);
        if (input.is_key_down(fr::Key::A))
            fps_camera.move_right(-speed);
        if (input.is_key_down(fr::Key::D))
            fps_camera.move_right(speed);

        fps_camera.add_yaw_pitch(input.mouse_delta_x * 0.1f, -input.mouse_delta_y * 0.1f);
        fps_camera.update();

        render_queue.clear_leftover();

        fr::RenderSystem::submit_meshes(world, render_queue, asset_manager, gbuffer_pipe);
        render_queue.sort();

        renderer.render(render_queue, window.get_width(), window.get_height(),
                        fps_camera.get_view_projection_matrix(), lighting_pipe);

        window.swap_buffers();
    }

    fr::destroy_opengl_render_device(device);
    fr::shutdown_core_ctx();

    return 0;
}
