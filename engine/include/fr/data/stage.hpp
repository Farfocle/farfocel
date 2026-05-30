/**
 * @file stage.hpp
 * @author Kiju
 *
 * @brief Stage enum and System type used by the ECS scheduling system.
 */

#pragma once

#include "fr/core/inline_function.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

class Scope;

// ==================================================================== Typedefs
using StageStorageType = U8;

enum class Stage : StageStorageType {
    PreUpdate,
    PreUpdateScript,
    Update,
    UpdateScript,
    PostUpdate,
    PostUpdateScript
};

constexpr U8 STAGE_COUNT = 6;

using System = Fn128<void(Scope &)>;

} // namespace fr
