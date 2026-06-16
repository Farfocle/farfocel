/**
 * @file world_actions.hpp
 * @author Tfoedy
 * @brief High-level world authoring actions used by runtime devtools.
 */

#pragma once

#include <utility>

#include "fr/core/ctx.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/thing.hpp"
#include "fr/devtools/editor_commands.hpp"
#include "fr/logger/logger.hpp"
#include "fr/scene/primitive_mesh_system.hpp"
#include "fr/scene/render_asset_system.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/transform_system.hpp"

namespace fr::devtools {

/**
 * @brief Initial transform used when spawning authored entities.
 */
struct SpawnTransformDesc {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

/**
 * @brief Builds a LocalTransformPart from spawn transform data.
 */
[[nodiscard]] inline LocalTransformPart
make_local_transform(const SpawnTransformDesc &desc) noexcept {
    LocalTransformPart transform{};
    transform.position = desc.position;
    transform.rotation = desc.rotation;
    transform.scale = desc.scale;
    return transform;
}

/**
 * @brief Spawns an empty authored entity with transform parts.
 */
inline Thing spawn_empty(EditorContext &ctx,
                         const SpawnTransformDesc &transform_desc = {}) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    Thing thing = ctx.world->spawn();

    LocalTransformPart local = make_local_transform(transform_desc);
    WorldTransformPart world_transform = TransformSystem::compose_world_transform(local, nullptr);

    ctx.world->emplace_now<RelationsPart>(thing);
    ctx.world->emplace_now<LocalTransformPart>(thing, local);
    ctx.world->emplace_now<WorldTransformPart>(thing, world_transform);

    TransformSystem::rebuild_world_transforms(*ctx.world);
    select_thing(ctx, thing);

    return thing;
}

/**
 * @brief Spawns a mesh renderer entity referencing a cooked logical .fmesh path.
 */
inline Thing spawn_mesh(EditorContext &ctx, StringView mesh_path,
                        const SpawnTransformDesc &transform_desc = {}) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    if (mesh_path.is_empty()) {
        FR_LOG_ERR("[DevTools] Cannot spawn mesh entity with empty mesh path.");
        return Thing::nil();
    }

    Thing thing = spawn_empty(ctx, transform_desc);

    MeshRendererPart mesh{};
    mesh.mesh_path = String::from_view(mesh_path);
    mesh.mesh_id = AssetId::from_logical_path(mesh.mesh_path.view());

    ctx.world->emplace_now<MeshRendererPart>(thing, std::move(mesh));

    TransformSystem::rebuild_world_transforms(*ctx.world);
    RenderAssetSystem::resolve(*ctx.world, *ctx.assets);
    select_thing(ctx, thing);

    return thing;
}

/**
 * @brief Spawns a camera entity.
 */
inline Thing spawn_camera(EditorContext &ctx,
                          const SpawnTransformDesc &transform_desc = {}) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    Thing thing = spawn_empty(ctx, transform_desc);

    ctx.world->emplace_now<CameraPart>(thing);
    ctx.world->emplace_now<FPSControllerPart>(thing);

    TransformSystem::rebuild_world_transforms(*ctx.world);
    select_thing(ctx, thing);

    return thing;
}

/**
 * @brief Spawns a directional light entity.
 */
inline Thing spawn_directional_light(EditorContext &ctx,
                                     const SpawnTransformDesc &transform_desc = {}) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    Thing thing = spawn_empty(ctx, transform_desc);

    ctx.world->emplace_now<DirectionalLightPart>(thing);

    TransformSystem::rebuild_world_transforms(*ctx.world);
    select_thing(ctx, thing);

    return thing;
}

/**
 * @brief Spawns a point light entity.
 */
inline Thing spawn_point_light(EditorContext &ctx,
                               const SpawnTransformDesc &transform_desc = {}) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    Thing thing = spawn_empty(ctx, transform_desc);

    ctx.world->emplace_now<PointLightPart>(thing);

    TransformSystem::rebuild_world_transforms(*ctx.world);
    select_thing(ctx, thing);

    return thing;
}

/**
 * @brief Spawns a spot light entity.
 */
inline Thing spawn_spot_light(EditorContext &ctx,
                              const SpawnTransformDesc &transform_desc = {}) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    Thing thing = spawn_empty(ctx, transform_desc);

    ctx.world->emplace_now<SpotLightPart>(thing);

    TransformSystem::rebuild_world_transforms(*ctx.world);
    select_thing(ctx, thing);

    return thing;
}

/**
 * @brief Spawns a persistent runtime primitive entity.
 */
inline Thing spawn_primitive(EditorContext &ctx, PrimitiveMeshKind kind,
                             const SpawnTransformDesc &transform_desc = {}) noexcept {
    FR_ASSERT(ctx.is_valid(), "EditorContext must be valid");

    Thing thing = spawn_empty(ctx, transform_desc);

    PrimitiveMeshPart primitive{};
    primitive.kind = static_cast<U32>(kind);

    ctx.world->emplace_now<PrimitiveMeshPart>(thing, primitive);
    ctx.world->emplace_now<MeshRendererPart>(thing);

    TransformSystem::rebuild_world_transforms(*ctx.world);
    PrimitiveMeshSystem::resolve(*ctx.world, *ctx.assets, get_ambient_ctx().alloc);
    select_thing(ctx, thing);

    return thing;
}

/**
 * @brief Spawns a cube primitive.
 */
inline Thing spawn_cube(EditorContext &ctx,
                        const SpawnTransformDesc &transform_desc = {}) noexcept {
    return spawn_primitive(ctx, PrimitiveMeshKind::Cube, transform_desc);
}

/**
 * @brief Spawns a plane primitive.
 */
inline Thing spawn_plane(EditorContext &ctx,
                         const SpawnTransformDesc &transform_desc = {}) noexcept {
    return spawn_primitive(ctx, PrimitiveMeshKind::Plane, transform_desc);
}

/**
 * @brief Spawns a grid primitive.
 */
inline Thing spawn_grid(EditorContext &ctx,
                        const SpawnTransformDesc &transform_desc = {}) noexcept {
    return spawn_primitive(ctx, PrimitiveMeshKind::Grid, transform_desc);
}

} // namespace fr::devtools
