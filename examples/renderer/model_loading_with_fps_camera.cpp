#include "fr/core/ctx.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/time.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/window.hpp"
#include "fr/renderer/mesh.hpp"
#include "fr/renderer/mesh_loader.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"
#include "fr/renderer/renderer.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"

#include <glm/glm.hpp>
#include <iostream>

enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT };

const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 400.f;
const float SENSITIVITY = 0.35f;
const float ZOOM = 60.0f;

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    float Yaw;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
          MovementSpeed(SPEED),
          MouseSensitivity(SENSITIVITY),
          Zoom(ZOOM) {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    void ProcessKeyboard(Camera_Movement direction, float deltaTime) {
        float velocity = MovementSpeed * deltaTime;
        if (direction == FORWARD)
            Position += Front * velocity;
        if (direction == BACKWARD)
            Position -= Front * velocity;
        if (direction == LEFT)
            Position -= Right * velocity;
        if (direction == RIGHT)
            Position += Right * velocity;
    }

    const glm::mat4 get_view_proj_matrix() const noexcept {
        glm::mat4 view = glm::lookAt(Position, Position + Front, Up);
        glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1920.0f / 1080.0f, 0.1f, 10000.0f);
        return proj * view;
    }

    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;
        Yaw += xoffset;
        Pitch += yoffset;
        if (constrainPitch) {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }
        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};

const char *VERTEX_SHADER = R"(
#version 450 core
layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_uv;

layout(std430, binding = 0) readonly buffer TransformBuffer {
    mat4 transforms[];
};

layout(std430, binding = 1) readonly buffer CameraBuffer {
    mat4 u_view_proj;
};

uniform uint u_transform_idx;

out vec3 v_normal;
void main() {
    v_normal = a_normal;
    mat4 model_matrix = transforms[u_transform_idx];
    gl_Position = u_view_proj * model_matrix * vec4(a_position, 1.0);
}
)";

const char *FRAGMENT_SHADER = R"(
#version 450 core
in vec3 v_normal;
out vec4 o_color;
void main() {
    o_color = vec4((normalize(v_normal) * 0.5) + 0.5, 1.0);
}
)";

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

void processInput(const fr::WindowInput &input, float deltaTime) {
    if (input.is_key_down(fr::Key::W))
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (input.is_key_down(fr::Key::S))
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (input.is_key_down(fr::Key::A))
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (input.is_key_down(fr::Key::D))
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

int main() {
    fr::init_core_ctx();
    {
        std::string paths;
        std::cout << "Enter path to a GLTF model: ";
        std::cin >> paths;
        std::cout << "\n";

        fr::String path = fr::String::from_chars(paths.c_str());

        fr::Window window;
        fr::WindowProperties windows_props{
            .title = "Farfocel Renderer Example (WASD + MOUSE to move, ESC to quit)",
            .width = 1280,
            .height = 720,
            .api = fr::GRAPHICS_API::OPENGL};

        if (!window.init(windows_props)) {
            std::cout << "window error: tough luck\n";
            return -1;
        }

        fr::RenderDevice *render_device =
            fr::create_opengl_render_device(fr::get_ambient_ctx().alloc);
        {
            fr::Renderer renderer(render_device);
            fr::RenderQueue render_queue(fr::get_ambient_ctx_mut().alloc);

            fr::ShaderHandle shader_handle = render_device->create_shader(
                fr::StringView(VERTEX_SHADER), fr::StringView(FRAGMENT_SHADER));
            fr::RenderPipelineProperties pipe_props{.shader = shader_handle,
                                                    .cull_mode = fr::CullMode::Back,
                                                    .depth_test = true,
                                                    .depth_write = true,
                                                    .wireframe = false};
            fr::RenderPipelineHandle pipe = render_device->create_render_pipeline(pipe_props);

            fr::MeshData mesh = fr::load_mesh_gltf(render_device, path);

            if (!mesh.vbo.is_valid()) {
                std::cout << "mesh error: tough luck\n";
                return -1;
            }

            fr::WindowInput global_input;

            U64 last_time = fr::time::get_steady_now_ms();
            bool running = true;

            while (running) {
                U64 current_time = fr::time::get_steady_now_ms();
                float dt = static_cast<float>(current_time - last_time) / 1000.0f;
                last_time = current_time;

                if (!window.poll_events(global_input)) {
                    running = false;
                }

                if (global_input.is_key_pressed(fr::Key::Escape)) {
                    running = false;
                }

                if (global_input.mouse_delta_x != 0.0f || global_input.mouse_delta_y != 0.0f) {
                    camera.ProcessMouseMovement(global_input.mouse_delta_x,
                                                -global_input.mouse_delta_y);
                }
                processInput(global_input, dt);

                render_queue.clear_leftover();

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::scale(model, glm::vec3(50.0f));

                for (USize i = 0; i < mesh.submeshes.size(); ++i) {
                    const auto &submesh = mesh.submeshes[i];
                    fr::DrawCall call{};
                    call.key = fr::SortKey::create(fr::RenderPassType::Opaque, pipe,
                                                   fr::TextureHandle{}, 10);
                    call.pipe = pipe;
                    call.vbo = mesh.vbo;
                    call.ibo = mesh.ibo;
                    call.index_count = submesh.index_count;
                    call.index_offset = submesh.index_offset;
                    call.vertex_offset = submesh.vertex_offset;
                    call.vbo_stride = sizeof(fr::Vertex);

                    render_queue.send_call(call, model * submesh.transform);
                }

                render_queue.sort();

                fr::TextureHandle depth_test{};
                renderer.render(render_queue, fr::Slice<const fr::TextureHandle>(), depth_test,
                                window.get_width(), window.get_height(),
                                camera.get_view_proj_matrix());
                window.swap_buffers();
            }

            render_device->destory_pipeline(pipe);
            render_device->destroy_shader(shader_handle);
            render_device->destroy_buffer(mesh.vbo);
            render_device->destroy_buffer(mesh.ibo);
        }
        fr::destroy_opengl_render_device(render_device);
    }
    fr::shutdown_core_ctx();
    return 0;
}
