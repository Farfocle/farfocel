/**
 * @file frustum.hpp
 * @author Tfoedy
 * @brief Camera frustum extraction and AABB visibility tests.
 */

#pragma once

#include <glm/glm.hpp>

#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief FrustumPlane used by frustum culling.
 */
struct FrustumPlane {
    glm::vec3 normal{0.0f};
    F32 distance{0.0f};

    /**
     * @brief Normalizes the plane equation.
     */
    void normalize() noexcept {
        const F32 length = glm::length(normal);

        if (length <= 0.000001f) {
            normal = glm::vec3(0.0f, 0.0f, 1.0f);
            distance = 0.0f;
            return;
        }

        normal /= length;
        distance /= length;
    }
};

/**
 * @brief Six clipping planes of a camera frustum.
 */
struct Frustum {
    FrustumPlane planes[6];

    /**
     * @brief Extracts frustum planes from a combined view-projection matrix.
     */
    static Frustum extract_from_matrix(const glm::mat4 &matrix) noexcept {
        Frustum f;

        f.planes[0].normal.x = matrix[0][3] + matrix[0][0];
        f.planes[0].normal.y = matrix[1][3] + matrix[1][0];
        f.planes[0].normal.z = matrix[2][3] + matrix[2][0];
        f.planes[0].distance = matrix[3][3] + matrix[3][0];

        f.planes[1].normal.x = matrix[0][3] - matrix[0][0];
        f.planes[1].normal.y = matrix[1][3] - matrix[1][0];
        f.planes[1].normal.z = matrix[2][3] - matrix[2][0];
        f.planes[1].distance = matrix[3][3] - matrix[3][0];

        f.planes[2].normal.x = matrix[0][3] + matrix[0][1];
        f.planes[2].normal.y = matrix[1][3] + matrix[1][1];
        f.planes[2].normal.z = matrix[2][3] + matrix[2][1];
        f.planes[2].distance = matrix[3][3] + matrix[3][1];

        f.planes[3].normal.x = matrix[0][3] - matrix[0][1];
        f.planes[3].normal.y = matrix[1][3] - matrix[1][1];
        f.planes[3].normal.z = matrix[2][3] - matrix[2][1];
        f.planes[3].distance = matrix[3][3] - matrix[3][1];

        f.planes[4].normal.x = matrix[0][3] + matrix[0][2];
        f.planes[4].normal.y = matrix[1][3] + matrix[1][2];
        f.planes[4].normal.z = matrix[2][3] + matrix[2][2];
        f.planes[4].distance = matrix[3][3] + matrix[3][2];

        f.planes[5].normal.x = matrix[0][3] - matrix[0][2];
        f.planes[5].normal.y = matrix[1][3] - matrix[1][2];
        f.planes[5].normal.z = matrix[2][3] - matrix[2][2];
        f.planes[5].distance = matrix[3][3] - matrix[3][2];

        for (int i = 0; i < 6; ++i) {
            f.planes[i].normalize();
        }

        return f;
    }

    /**
     * @brief Returns true if an AABB intersects the frustum.
     */
    [[nodiscard]] bool is_aabb_visible(const glm::vec3 &min_extents,
                                       const glm::vec3 &max_extents) const noexcept {
        for (int i = 0; i < 6; ++i) {
            glm::vec3 positive_vertex = min_extents;

            if (planes[i].normal.x >= 0.0f) {
                positive_vertex.x = max_extents.x;
            }

            if (planes[i].normal.y >= 0.0f) {
                positive_vertex.y = max_extents.y;
            }

            if (planes[i].normal.z >= 0.0f) {
                positive_vertex.z = max_extents.z;
            }

            if (glm::dot(planes[i].normal, positive_vertex) + planes[i].distance < 0.0f) {
                return false;
            }
        }

        return true;
    }
};

} // namespace fr
