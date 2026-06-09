/**
 * @file frustum.hpp
 * @brief Camera frustum extraction and AABB visibility testing.
 *
 * @details
 * This file provides a small frustum representation used for CPU-side visibility culling.
 */
#pragma once

#include "fr/core/typedefs.hpp"
#include <glm/glm.hpp>

namespace fr {

struct Plane {
    glm::vec3 normal;
    F32 distance;

    void normalize() noexcept {
        F32 length = glm::length(normal);
        normal /= length;
        distance /= length;
    }
};

/**
 * @brief Represents the 6 clipping planes of a camera view.
 */
struct Frustum {
    Plane planes[6];

    /**
     * @brief Extracts view frustum planes directly from the combined View-Projection matrix.
     * Utilizes the Gribb/Hartmann fast extraction method. Generally speaking some crazy stuff.
     */
    static Frustum extract_from_matrix(const glm::mat4 &matrix) noexcept {
        Frustum f;

        // Left Plane
        f.planes[0].normal.x = matrix[0][3] + matrix[0][0];
        f.planes[0].normal.y = matrix[1][3] + matrix[1][0];
        f.planes[0].normal.z = matrix[2][3] + matrix[2][0];
        f.planes[0].distance = matrix[3][3] + matrix[3][0];

        // Right Plane
        f.planes[1].normal.x = matrix[0][3] - matrix[0][0];
        f.planes[1].normal.y = matrix[1][3] - matrix[1][0];
        f.planes[1].normal.z = matrix[2][3] - matrix[2][0];
        f.planes[1].distance = matrix[3][3] - matrix[3][0];

        // Bottom Plane
        f.planes[2].normal.x = matrix[0][3] + matrix[0][1];
        f.planes[2].normal.y = matrix[1][3] + matrix[1][1];
        f.planes[2].normal.z = matrix[2][3] + matrix[2][1];
        f.planes[2].distance = matrix[3][3] + matrix[3][1];

        // Top Plane
        f.planes[3].normal.x = matrix[0][3] - matrix[0][1];
        f.planes[3].normal.y = matrix[1][3] - matrix[1][1];
        f.planes[3].normal.z = matrix[2][3] - matrix[2][1];
        f.planes[3].distance = matrix[3][3] - matrix[3][1];

        // Near Plane
        f.planes[4].normal.x = matrix[0][3] + matrix[0][2];
        f.planes[4].normal.y = matrix[1][3] + matrix[1][2];
        f.planes[4].normal.z = matrix[2][3] + matrix[2][2];
        f.planes[4].distance = matrix[3][3] + matrix[3][2];

        // Far Plane
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
     * @brief Tests whether an axis-aligned bounding box intersects the frustum.
     *
     * @param min_extents Minimum AABB extents.
     * @param max_extents Maximum AABB extents.
     * @return True if the box is at least partially visible.
     */

    bool is_aabb_visible(const glm::vec3 &min_extents,
                         const glm::vec3 &max_extents) const noexcept {
        for (int i = 0; i < 6; ++i) {
            // Find the point on the AABB that is furthest along the plane's normal
            glm::vec3 positive_vertex = min_extents;
            if (planes[i].normal.x >= 0.0f)
                positive_vertex.x = max_extents.x;
            if (planes[i].normal.y >= 0.0f)
                positive_vertex.y = max_extents.y;
            if (planes[i].normal.z >= 0.0f)
                positive_vertex.z = max_extents.z;

            // If the furthest point is still behind the plane, the entire AABB is behind it
            // (which essentially means that is it indeed Culled)
            if (glm::dot(planes[i].normal, positive_vertex) + planes[i].distance < 0.0f) {
                return false;
            }
        }
        return true;
    }
};

} // namespace fr
