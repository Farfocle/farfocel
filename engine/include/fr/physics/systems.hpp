/**
 * @file systems.hpp
 * @author Kiju
 *
 * @brief This file contains the rigid body physics systems.
 */

#pragma once

#include "fr/data/world.hpp"

namespace fr {
/// @brief Applies forces to all rigid bodies in the world.
void rigit_body_force_system(Scope scope);

/// @brief Performs the broadphase collision detection.
void broadphase_collision_detection_system(Scope scope);

/// @brief Performs the narrowphase collision detection.
void narrowphase_collision_detection_system(Scope scope);

/// @brief Solves constraints for all rigid bodies in the world.
void rigit_body_collision_resolution_system(Scope scope);
} // namespace fr
