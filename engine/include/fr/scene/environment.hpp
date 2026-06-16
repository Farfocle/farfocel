/**
 * @file environment_system.hpp
 * @author Tfoedy
 * @brief Runtime environment texture resolve/release system.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/data/world.hpp"
#include "fr/logger/logger.hpp"

namespace fr {

/**
 * @brief Scene-owned environment texture reference.
 *
 * @details
 * texture_path is persistent scene data and should point to a cooked logical .ftex path.
 * Runtime texture handles are managed by resolve_env/release_env and intentionally not serialized.
 */
struct EnvironmentState {
    String texture_path{};
    AssetId texture_id{};

    AssetId resolved_texture_id{};
    TextureAssetHandle texture_handle{};

    bool enabled{false};

    EnvironmentState() noexcept = default;

    explicit EnvironmentState(StringView path)
        : texture_path(String::from_view(path)),
          texture_id(AssetId::from_logical_path(path)) {
    }

    [[nodiscard]] bool is_resolved() const noexcept {
        return texture_handle.is_valid();
    }

    FR_SHAPE({
        FR_PROP(texture_path);
        FR_PROP(enabled);
    })
};

inline void release_environment(EnvironmentState &env, AssetManager &assets) noexcept {
    if (env.texture_handle.is_valid()) {
        assets.unload_texture(env.texture_handle);
    }

    env.texture_handle = {};
    env.resolved_texture_id = {};
}

inline void resolve_environment(EnvironmentState &env, AssetManager &assets) noexcept {
    if (!env.enabled) {
        release_environment(env, assets);
        return;
    }

    if (env.texture_path.size() != 0) {
        env.texture_id = AssetId::from_logical_path(env.texture_path.view());
    }

    if (!env.texture_id.is_valid()) {
        release_environment(env, assets);
        return;
    }

    if (env.texture_handle.is_valid() && env.resolved_texture_id == env.texture_id) {
        return;
    }

    release_environment(env, assets);

    TextureAssetHandle loaded = assets.load_texture(env.texture_id);
    if (!loaded.is_valid()) {
        FR_LOG_ERR("[env] Failed to resolve environment texture. path='{}', id={}",
                   env.texture_path.view(), env.texture_id.value);
        return;
    }

    env.texture_handle = loaded;
    env.resolved_texture_id = env.texture_id;
}

inline void resolve_environment(World &world, AssetManager &assets) noexcept {
    if (EnvironmentState *env = world.try_get_resource<EnvironmentState>()) {
        resolve_environment(*env, assets);
    }
}

inline void release_environment(World &world, AssetManager &assets) noexcept {
    if (EnvironmentState *env = world.try_get_resource<EnvironmentState>()) {
        release_environment(*env, assets);
    }
}

inline TextureHandle get_active_environment_texture(World &world,
                                                    const AssetManager &assets) noexcept {
    const EnvironmentState *env = world.try_get_resource<EnvironmentState>();
    if (!env || !env->enabled || !env->texture_handle.is_valid()) {
        return {};
    }

    return assets.get_texture_handle(env->texture_handle);
}

} // namespace fr
