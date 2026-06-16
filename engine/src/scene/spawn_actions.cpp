/**
 * @file spawn_actions.cpp
 * @brief Scene-level thing spawn action implementations.
 */

#include "fr/scene/spawn_actions.hpp"

#include <utility>

namespace fr {

Thing spawn_base(Scope scope, const LocalTransformPart &local) noexcept {
    Thing thing = scope.spawn();
    scope.insert(thing, RelationsPart{});
    scope.insert(thing, local);
    return thing;
}

Thing spawn_mesh(Scope scope, StringView mesh_path, const LocalTransformPart &local) noexcept {
    if (mesh_path.is_empty()) {
        return Thing::nil();
    }

    Thing thing = spawn_base(scope, local);

    MeshRendererPart mesh{};
    mesh.mesh_path = String::from_view(mesh_path);
    mesh.mesh_id = AssetId::from_logical_path(mesh.mesh_path.view());

    scope.insert(thing, std::move(mesh));
    return thing;
}

Thing spawn_camera(Scope scope, const LocalTransformPart &local) noexcept {
    Thing thing = spawn_base(scope, local);
    scope.insert(thing, CameraPart{});
    scope.insert(thing, FPSControllerPart{});
    return thing;
}

Thing spawn_directional_light(Scope scope, const LocalTransformPart &local) noexcept {
    Thing thing = spawn_base(scope, local);
    scope.insert(thing, DirectionalLightPart{});
    return thing;
}

Thing spawn_point_light(Scope scope, const LocalTransformPart &local) noexcept {
    Thing thing = spawn_base(scope, local);
    scope.insert(thing, PointLightPart{});
    return thing;
}

Thing spawn_spot_light(Scope scope, const LocalTransformPart &local) noexcept {
    Thing thing = spawn_base(scope, local);
    scope.insert(thing, SpotLightPart{});
    return thing;
}

Thing spawn_primitive(Scope scope, PrimitiveMeshKind kind,
                      const LocalTransformPart &local) noexcept {
    Thing thing = spawn_base(scope, local);

    PrimitiveMeshPart primitive{};
    primitive.kind = static_cast<U32>(kind);

    scope.insert(thing, primitive);
    scope.insert(thing, MeshRendererPart{});
    return thing;
}

Thing spawn_cube(Scope scope, const LocalTransformPart &local) noexcept {
    return spawn_primitive(scope, PrimitiveMeshKind::Cube, local);
}

Thing spawn_plane(Scope scope, const LocalTransformPart &local) noexcept {
    return spawn_primitive(scope, PrimitiveMeshKind::Plane, local);
}

Thing spawn_grid(Scope scope, const LocalTransformPart &local) noexcept {
    return spawn_primitive(scope, PrimitiveMeshKind::Grid, local);
}

} // namespace fr
