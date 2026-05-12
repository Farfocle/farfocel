/**
 * @file part.hpp
 * @author Kiju
 *
 * @brief Archetype, some part specific constants.
 */

#pragma once

#include "fr/core/bitset.hpp"

namespace fr {
constexpr USize max_parts = 128;

/**
 * @brief Represents the parts a thing is made out of. Each bit signals if a specific Part is
 * attatched to the Thing, index to these bits are TypeIdx of the Parts.
 */
struct Archetype {
    Bitset<max_parts> bits{};
};
} // namespace fr
