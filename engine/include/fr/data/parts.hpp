/**
 * @file parts.hpp
 * @author Kiju
 *
 * @brief Commonly used parts.
 */

#pragma once

#include "fr/core/math.hpp"
#include "fr/core/meta.hpp"
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

    template <typename Archive>
    void shape(Archive &archive) noexcept {
        if constexpr (Archive::action == ArchiveAction::Write) {
            Thing p = parent;
            Thing fc = first_child;
            Thing ps = prev_sibling;
            Thing ns = next_sibling;
            HierarchyDepth d = depth;
            archive.prop("parent", p);
            archive.prop("first_child", fc);
            archive.prop("prev_sibling", ps);
            archive.prop("next_sibling", ns);
            archive.prop("depth", d);
        } else {
            archive.prop("parent", parent);
            archive.prop("first_child", first_child);
            archive.prop("prev_sibling", prev_sibling);
            archive.prop("next_sibling", next_sibling);
            archive.prop("depth", depth);
        }
    }
};

// ================================================================== Transforms

/**
 * @brief The transform of a thing relative to its parent (or world origin if it has none).
 *
 * @details Decomposed form: position, orientation quaternion, and per-axis scale.
 * The physics and rendering systems reconstruct a matrix from these as needed via `to_mat4()`.
 */
struct LocalTransformPart {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    FR_SHAPE({
        FR_PROP(position);
        FR_PROP(rotation);
        FR_PROP(scale);
    })

    /// @brief Returns a transform at the origin with identity rotation and unit scale.
    static LocalTransformPart identity() noexcept {
        return {};
    }

    /// @brief Returns a transform at `p` with identity rotation and unit scale.
    static LocalTransformPart from_pos(Vec3 p) noexcept {
        LocalTransformPart t;
        t.position = p;
        return t;
    }

    /**
     * @brief Builds the TRS matrix: T * R * S.
     * @note Column 3 carries the translation; the upper-left 3x3 is R*S.
     */
    Mat4 to_mat4() const noexcept {
        Mat4 m = glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        m[3] = Vec4(position, 1.0f);

        return m;
    }
};

/**
 * @brief The fully composed world-space transform of a thing.
 *
 * @details Computed by the transform propagation system from the `LocalTransformPart` hierarchy.
 * `mat` is the cached TRS matrix ready to upload to the GPU.
 * `position` and `rotation` are kept separate for physics queries that need world
 * position/orientation without decomposing the matrix.
 */
struct WorldTransformPart {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Mat4 matrix{1.0f};

    WorldTransformPart() noexcept = default;

    FR_SHAPE({
        FR_PROP(position);
        FR_PROP(rotation);
        FR_PROP(scale);
        FR_PROP(matrix);
    })

    /// @brief Returns an identity world transform.
    static WorldTransformPart identity() noexcept {
        return {};
    }
};
} // namespace fr

FR_TYPE(fr::RelationsPart);
FR_TYPE(fr::WorldTransformPart);
FR_TYPE(fr::LocalTransformPart);
