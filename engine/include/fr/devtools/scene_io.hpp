/**
 * @file scene_io.hpp
 * @author Tfoedy
 * @brief Scene save/load helpers for runtime devtools.
 */

#pragma once

#include <utility>

#include "fr/asset/asset_manager.hpp"
#include "fr/core/file.hpp"
#include "fr/core/json.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"
#include "fr/logger/logger.hpp"
#include "fr/scene/render_asset_system.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/transform_system.hpp"

namespace fr::devtools {

/**
 * @brief Ensures built-in scene part pools required by Farfocel devtools scenes.
 *
 * @details
 * Registry deserialization can only read part pools that already exist. This helper registers the
 * common engine scene/render parts used by authored worlds.
 */
inline void ensure_default_scene_part_types(World &world) noexcept {
    world.ensure<RelationsPart>();
    world.ensure<LocalTransformPart>();
    world.ensure<WorldTransformPart>();

    world.ensure<CameraPart>();
    world.ensure<FPSControllerPart>();

    world.ensure<MeshRendererPart>();
    world.ensure<MaterialOverridePart>();

    world.ensure<PointLightPart>();
    world.ensure<SpotLightPart>();
    world.ensure<DirectionalLightPart>();
}

/**
 * @brief Saves persistent scene data to JSON.
 *
 * @details
 * This intentionally serializes only world registry data. Runtime resources are not saved because
 * they often contain non-owning pointers to application services.
 */
inline bool save_scene(World &world, StringView output_path) noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[SceneIO] Cannot save scene to an empty path.");
        return false;
    }

    JsonWriterArchive writer({.pretty = true});
    world.shape_scene(writer);

    String json = writer.consume();
    if (json.size() == 0) {
        FR_LOG_ERR("[SceneIO] Failed to serialize scene: {}", output_path);
        return false;
    }

    String path = String::from_view(output_path);

    if (!file::ensure_parent_directory(path.view())) {
        FR_LOG_ERR("[SceneIO] Failed to create scene output directory: {}", output_path);
        return false;
    }

    const Slice<const Byte> bytes(reinterpret_cast<const Byte *>(json.data()), json.size());

    if (!file::write_all_bytes(path, bytes)) {
        FR_LOG_ERR("[SceneIO] Failed to write scene file: {}", output_path);
        return false;
    }

    FR_LOG("[SceneIO] Saved scene: {}", output_path);
    return true;
}

/**
 * @brief Loads persistent scene data into an already empty/prepared world.
 *
 * @warning
 * This does not clear the current world. Prefer load_scene_replacing_world() for editor/runtime
 * reload workflows.
 */
inline bool load_scene_into_prepared_world(World &world, AssetManager &assets,
                                           StringView input_path) noexcept {
    if (input_path.is_empty()) {
        FR_LOG_ERR("[SceneIO] Cannot load scene from an empty path.");
        return false;
    }

    if (world.alive_thing_count() != 0) {
        FR_LOG_ERR("[SceneIO] Refusing to load scene into a non-empty world. "
                   "Use load_scene_replacing_world() instead.");
        return false;
    }

    String path = String::from_view(input_path);

    auto text = file::read_all_text(path);
    if (!text.is_some()) {
        FR_LOG_ERR("[SceneIO] Failed to read scene file: {}", input_path);
        return false;
    }

    ensure_default_scene_part_types(world);

    String json = std::move(text.unwrap());

    JsonReaderArchive reader(json.view());
    world.shape_scene(reader);

    if (!reader.consume()) {
        FR_LOG_ERR("[SceneIO] Failed to deserialize scene: {}", input_path);
        return false;
    }

    TransformSystem::rebuild_world_transforms(world);
    RenderAssetSystem::resolve(world, assets);

    FR_LOG("[SceneIO] Loaded scene: {}", input_path);
    return true;
}

/**
 * @brief Replaces current scene data with persistent scene data loaded from JSON.
 *
 * @details
 * Runtime resources and scheduled systems are preserved. Existing render asset handles are
 * released, all scene things are cleared, scene registry data is deserialized and render assets are
 * resolved.
 */
inline bool load_scene_replacing_world(World &world, AssetManager &assets,
                                       StringView input_path) noexcept {
    if (input_path.is_empty()) {
        FR_LOG_ERR("[SceneIO] Cannot load scene from an empty path.");
        return false;
    }

    String path = String::from_view(input_path);

    auto text = file::read_all_text(path);
    if (!text.is_some()) {
        FR_LOG_ERR("[SceneIO] Failed to read scene file: {}", input_path);
        return false;
    }

    RenderAssetSystem::release(world, assets);
    world.clear_scene();

    ensure_default_scene_part_types(world);

    String json = std::move(text.unwrap());

    JsonReaderArchive reader(json.view());
    world.shape_scene(reader);

    if (!reader.consume()) {
        FR_LOG_ERR("[SceneIO] Failed to deserialize scene: {}", input_path);
        return false;
    }

    TransformSystem::rebuild_world_transforms(world);
    RenderAssetSystem::resolve(world, assets);

    FR_LOG("[SceneIO] Replaced scene from file: {}", input_path);
    return true;
}

} // namespace fr::devtools
