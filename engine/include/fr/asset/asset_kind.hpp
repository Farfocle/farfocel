/**
 * @file asset_kind.hpp
 * @author Tfoedy
 * @brief Cooked asset kind classification.
 */

#pragma once

#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief High-level cooked asset kind.
enum class AssetKind : U32 {
    Unknown = 0,
    Mesh,
    Texture,
    Shader,
    Material,
};

} // namespace fr
