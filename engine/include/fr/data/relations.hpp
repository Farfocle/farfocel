/**
 * @file relations.hpp
 * @author Kiju
 *
 * @brief Relations part and its utils.
 */

#pragma once

#include "fr/core/shape.hpp"
#include "fr/data/thing.hpp"

namespace fr {
using HierarchyDepth = U32;
constexpr HierarchyDepth ROOT_HIERARCHY_DEPTH = 0;
constexpr HierarchyDepth MAX_HIERARCHY_DEPTH = std::numeric_limits<HierarchyDepth>::max();

struct Relations {
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
} // namespace fr
