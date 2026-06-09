/**
 * @file parts.hpp
 * @author Kiju
 *
 * @brief Commonly used parts.
 */

#pragma once

#include "fr/core/math.hpp"
#include "fr/core/shape.hpp"
#include "fr/data/thing.hpp"
#include "glm/gtc/quaternion.hpp"

namespace fr {

// =================================================================== Relations

using HierarchyDepth = U32;
constexpr HierarchyDepth ROOT_HIERARCHY_DEPTH = 0;
constexpr HierarchyDepth MAX_HIERARCHY_DEPTH = std::numeric_limits<HierarchyDepth>::max();

struct RelationsPart {
    Thing parent{Thing::nil()};
    Thing first_child{Thing::nil()};
    Thing prev_sibling{Thing::nil()};
    Thing next_sibling{Thing::nil()};
    HierarchyDepth depth{ROOT_HIERARCHY_DEPTH};

    FR_SHAPE({
        FR_PROP(parent);
        FR_PROP(first_child);
        FR_PROP(prev_sibling);
        FR_PROP(next_sibling);
        FR_PROP(depth);
    })
};

// ================================================================== Transforms

/**
 * @brief The transform of a thing relative to its parent (or world origin if it has none).
 *
 * @details Decomposed form: position, orientation quaternion, and per-axis scale.
 * The physics and rendering systems reconstruct a matrix from these as needed via `to_mat4()`.
 */
struct LocalTransformPart {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    FR_SHAPE({
        FR_PROP(pos);
        FR_PROP(rot);
        FR_PROP(scale);
    })

    /// @brief Returns a transform at the origin with identity rotation and unit scale.
    static LocalTransformPart identity() noexcept {
        return {};
    }

    /// @brief Returns a transform at `p` with identity rotation and unit scale.
    static LocalTransformPart from_pos(Vec3 p) noexcept {
        LocalTransformPart t;
        t.pos = p;
        return t;
    }

    /**
     * @brief Builds the TRS matrix: T * R * S.
     * @note Column 3 carries the translation; the upper-left 3x3 is R*S.
     */
    Mat4 to_mat4() const noexcept {
        Mat4 m = glm::mat4_cast(rot);
        m = glm::scale(m, scale);
        m[3] = Vec4(pos, 1.0f);

        return m;
    }
};

/**
 * @brief The fully composed world-space transform of a thing.
 *
 * @details Computed by the transform propagation system from the @ref LocalTransform hierarchy.
 * `mat` is the cached TRS matrix ready to upload to the GPU.
 * `pos` and `rot` are kept separate for physics queries that need world position/orientation
 * without decomposing the matrix.
 */
struct WorldTransformPart {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    Mat4 mat{1.0f};

    FR_SHAPE({
        FR_PROP(pos);
        FR_PROP(rot);
        FR_PROP(mat);
    })

    /// @brief Returns an identity world transform.
    static WorldTransformPart identity() noexcept {
        return {};
    }
};
} // namespace fr
