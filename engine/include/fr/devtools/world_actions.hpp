/**
 * @file world_actions.hpp
 * @brief Devtools spawn wrappers that delegate to scene spawn actions and select the result.
 */

#pragma once

#include "fr/core/string_view.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
#include "fr/devtools/state.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/spawn_actions.hpp"

namespace fr::devtools {

Thing spawn_base(World &world, DevToolsState &tools, const LocalTransformPart &local = {}) noexcept;

Thing spawn_mesh(World &world, DevToolsState &tools, StringView mesh_path,
                 const LocalTransformPart &local = {}) noexcept;

Thing spawn_camera(World &world, DevToolsState &tools,
                   const LocalTransformPart &local = {}) noexcept;

Thing spawn_directional_light(World &world, DevToolsState &tools,
                              const LocalTransformPart &local = {}) noexcept;

Thing spawn_point_light(World &world, DevToolsState &tools,
                        const LocalTransformPart &local = {}) noexcept;

Thing spawn_spot_light(World &world, DevToolsState &tools,
                       const LocalTransformPart &local = {}) noexcept;

Thing spawn_primitive(World &world, DevToolsState &tools, PrimitiveMeshKind kind,
                      const LocalTransformPart &local = {}) noexcept;

Thing spawn_cube(World &world, DevToolsState &tools, const LocalTransformPart &local = {}) noexcept;

Thing spawn_plane(World &world, DevToolsState &tools,
                  const LocalTransformPart &local = {}) noexcept;

Thing spawn_grid(World &world, DevToolsState &tools, const LocalTransformPart &local = {}) noexcept;

} // namespace fr::devtools
