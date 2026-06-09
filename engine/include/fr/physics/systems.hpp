/**
 * @file systems.hpp
 * @author Kiju
 *
 * @brief This file contains the physics systems.
 */

#pragma once

#include "fr/data/world.hpp"

namespace fr {
/**
 * @brief Applies forces to all rigid bodies in the world.
 */
void rigit_body_forces_system(Scope scope);

/**
 * @brief Performs the broadphase collision detection.
 */
void broadphase_collision_detection_system(Scope scope);

/**
 * @brief Performs the narrowphase collision detection.
 */
void narrowphase_collision_detection_system(Scope scope);

/**
 * @brief Solves constraints for all rigid bodies in the world.
 */
void rigit_body_contraist_system(Scope scope);

/**
 * @brief Integrates all rigid bodies in the world.
 */
void rigit_body_integration_system(Scope scope);
} // namespace fr
