/**
 * @file cmd.hpp
 * @author Kiju
 *
 * @brief Commands for the data layer.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr {

/**
 * @brief Kind of command that targets a part.
 */
enum class CmdKind : U8 { DestroyPart, InsertPart, MutatePart };

/**
 * @brief Command to destroy a part owned by a thing.
 * @tparam T Part type.
 */
template <typename T>
struct DestroyPartCmd {
    /// @brief Part alias for tooling and reflection.
    using Part = T;

    /// @brief Target thing.
    Thing thing;
};

/**
 * @brief Command to insert a part for a thing.
 * @tparam T Part type.
 */
template <typename T>
struct InsertPartCmd {
    /// @brief Part alias for tooling and reflection.
    using Part = T;

    /// @brief Target thing.
    Thing thing;

    /// @brief Part payload.
    Part part;
};

/**
 * @brief Command to mutate a part for a thing.
 * @tparam T Part type.
 */
template <typename T>
struct MutatePartCmd {
    /// @brief Part alias for tooling and reflection.
    using Part = T;

    /// @brief Target thing.
    Thing thing;

    /// @brief Previous part state.
    Part prev;

    /// @brief Next part state.
    Part next;
};
} // namespace fr
