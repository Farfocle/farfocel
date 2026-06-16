/**
 * @file world_actions.cpp
 * @brief Devtools spawn wrapper implementations.
 */

#include "fr/devtools/world_actions.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/scene/spawn_actions.hpp"

#include "fr/logger/logger.hpp"

namespace fr::devtools {

Thing spawn_base(World &world, DevToolsState &tools, const LocalTransformPart &local) noexcept {
    Thing thing = fr::spawn_base(Scope(&world), local);
    select_thing(tools, thing);
    return thing;
}

Thing spawn_mesh(World &world, DevToolsState &tools, StringView mesh_path,
                 const LocalTransformPart &local) noexcept {
    if (mesh_path.is_empty()) {
        FR_LOG_ERR("[DevTools] Cannot spawn mesh with empty path.");
        return Thing::nil();
    }

    Thing thing = fr::spawn_mesh(Scope(&world), mesh_path, local);
    select_thing(tools, thing);
    return thing;
}

Thing spawn_camera(World &world, DevToolsState &tools, const LocalTransformPart &local) noexcept {
    Thing thing = fr::spawn_camera(Scope(&world), local);
    select_thing(tools, thing);
    return thing;
}

Thing spawn_directional_light(World &world, DevToolsState &tools,
                              const LocalTransformPart &local) noexcept {
    Thing thing = fr::spawn_directional_light(Scope(&world), local);
    select_thing(tools, thing);
    return thing;
}

Thing spawn_point_light(World &world, DevToolsState &tools,
                        const LocalTransformPart &local) noexcept {
    Thing thing = fr::spawn_point_light(Scope(&world), local);
    select_thing(tools, thing);
    return thing;
}

Thing spawn_spot_light(World &world, DevToolsState &tools,
                       const LocalTransformPart &local) noexcept {
    Thing thing = fr::spawn_spot_light(Scope(&world), local);
    select_thing(tools, thing);
    return thing;
}

Thing spawn_primitive(World &world, DevToolsState &tools, PrimitiveMeshKind kind,
                      const LocalTransformPart &local) noexcept {
    Thing thing = fr::spawn_primitive(Scope(&world), kind, local);
    select_thing(tools, thing);
    return thing;
}

Thing spawn_cube(World &world, DevToolsState &tools, const LocalTransformPart &local) noexcept {
    return spawn_primitive(world, tools, PrimitiveMeshKind::Cube, local);
}

Thing spawn_plane(World &world, DevToolsState &tools, const LocalTransformPart &local) noexcept {
    return spawn_primitive(world, tools, PrimitiveMeshKind::Plane, local);
}

Thing spawn_grid(World &world, DevToolsState &tools, const LocalTransformPart &local) noexcept {
    return spawn_primitive(world, tools, PrimitiveMeshKind::Grid, local);
}

} // namespace fr::devtools
