// THIS IS A TEMPORARY CAMERA CLASS
// TO BE REPLACED WITH A PROPER ECS ONE

#pragma once

#include "fr/core/typedefs.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <algorithm>

namespace fr {
class Camera {
public:
    Camera(glm::vec3 start_pos, F32 fov, F32 aspect_ratio)
        : m_position(start_pos),
          m_yaw(-90.0f),
          m_pitch(0.0f),
          m_front(0.0f, 0.0f, -1.0f),
          m_up(0.0f, 1.0f, 0.0f),
          m_right(1.0f, 0.0f, -1.0f),
          m_fov(fov),
          m_aspect_ratio(aspect_ratio),
          m_near_plane(0.0001f),
          m_far_plane(1000.0f),
          m_view(1.0f),
          m_proj(1.0f),
          m_view_proj(1.0f) {

        calc_vectors();
        update();
    }

    void update() noexcept {
        m_view = glm::lookAt(m_position, m_position + m_front, m_up);
        m_proj = glm::perspective(glm::radians(m_fov), m_aspect_ratio, m_near_plane, m_far_plane);
        m_view_proj = m_proj * m_view;
    }

    void move_forward(F32 distance) noexcept {
        m_position += m_front * distance;
    }

    void move_right(F32 distance) noexcept {
        m_position += m_right * distance;
    }

    void move_up(F32 distance) noexcept {
        m_position += glm::vec3(0.0f, 1.0f, 0.0f) * distance;
    }

    void add_yaw_pitch(F32 yaw_offset, F32 pitch_offset) noexcept {
        m_yaw += yaw_offset;
        m_pitch += pitch_offset;
        m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

        calc_vectors();
    }

    void set_aspect_ratio(F32 aspect_ratio) noexcept {
        m_aspect_ratio = aspect_ratio;
    }

    const glm::mat4 &get_view_matrix() const noexcept {
        return m_view;
    }
    const glm::mat4 &get_projection_matrix() const noexcept {
        return m_proj;
    }
    const glm::mat4 &get_view_projection_matrix() const noexcept {
        return m_view_proj;
    }
    const glm::vec3 &get_position() const noexcept {
        return m_position;
    }

private:
    glm::vec3 m_position;

    F32 m_yaw;
    F32 m_pitch;

    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;

    F32 m_fov;
    F32 m_aspect_ratio;
    // frustrum
    F32 m_near_plane;
    F32 m_far_plane;

    glm::mat4 m_view;
    glm::mat4 m_proj;
    glm::mat4 m_view_proj;

    void calc_vectors() noexcept {
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

        m_front = glm::normalize(front);
        m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
        m_up = glm::normalize(glm::cross(m_right, m_front));
    }
};
} // namespace fr
