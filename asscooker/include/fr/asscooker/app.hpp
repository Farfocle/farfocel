/**
 * @file app.hpp
 * @brief Redirect shim — RendererApp has moved to fr/scene/app.hpp.
 *
 * @details
 * fr::scene::RendererApp is the canonical owner of the rendering infrastructure.
 * The asscooker layer provides fr::asscooker::cook_and_register_shaders and
 * fr::asscooker::load_dev_manifest_if_exists for dev-time shader cooking;
 * call those after app.init() and before the main loop.
 */

#pragma once

#include "fr/scene/app.hpp"
