/**
 * @file camera_system.hpp
 * @author Tfoedy
 * @brief System responsible for updating camera controllers.
 */

#pragma once

#include "fr/data/world.hpp"
#include "fr/platform/input.hpp"
#include "fr/scene/render_parts.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace fr {

class CameraSystem {
public:
    static void update_fps_cameras(World &world, const WindowInput &input, float dt) noexcept {
        for (auto [thing, fps, trans] : world.query<FPSControllerPart, TransformPart>()) {

            fps.yaw -= input.mouse_delta_x * fps.mouse_sensitivity;
            fps.pitch -= input.mouse_delta_y * fps.mouse_sensitivity;
            fps.pitch = std::clamp(fps.pitch, -89.0f, 89.0f);

            // quaterion transformation
            glm::vec3 euler(glm::radians(fps.pitch), glm::radians(fps.yaw), 0.0f);
            trans.rotation = glm::quat(euler);

            glm::vec3 forward = trans.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 right = trans.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

            float speed = fps.move_speed * dt;
            if (input.is_key_down(Key::W))
                trans.position += forward * speed;
            if (input.is_key_down(Key::S))
                trans.position -= forward * speed;
            if (input.is_key_down(Key::A))
                trans.position -= right * speed;
            if (input.is_key_down(Key::D))
                trans.position += right * speed;
            if (input.is_key_down(Key::Space))
                trans.position += up * speed;
            if (input.is_key_down(Key::LShift))
                trans.position -= up * speed;
        }
    }
};

} // namespace fr
