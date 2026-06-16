/**
 * @file environment_system.hpp
 * @author Tfoedy
 * @brief Runtime environment texture resolve/release system.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/data/world.hpp"
#include "fr/logger/logger.hpp"
#include "fr/scene/environment_parts.hpp"

namespace fr {

class EnvironmentSystem {
public:
    static void resolve(World &world, AssetManager &assets) noexcept {
        world.for_each_alive_thing([&](Thing thing) noexcept {
            if (thing.is_nil()) {
                return;
            }

            EnvironmentPart *env = world.try_get<EnvironmentPart>(thing);
            if (!env) {
                return;
            }

            resolve_one(*env, assets);
        });
    }

    static void release(World &world, AssetManager &assets) noexcept {
        world.for_each_alive_thing([&](Thing thing) noexcept {
            if (thing.is_nil()) {
                return;
            }

            EnvironmentPart *env = world.try_get<EnvironmentPart>(thing);
            if (!env) {
                return;
            }

            release_one(*env, assets);
        });
    }

    static TextureHandle active_environment_texture(World &world,
                                                    const AssetManager &assets) noexcept {
        TextureHandle out{};

        world.for_each_alive_thing([&](Thing thing) noexcept {
            if (out.is_valid() || thing.is_nil()) {
                return;
            }

            EnvironmentPart *env = world.try_get<EnvironmentPart>(thing);
            if (!env || !env->enabled || !env->texture_handle.is_valid()) {
                return;
            }

            out = assets.get_texture_handle(env->texture_handle);
        });

        return out;
    }

private:
    static void resolve_one(EnvironmentPart &env, AssetManager &assets) noexcept {
        if (!env.enabled) {
            release_one(env, assets);
            return;
        }

        if (env.texture_path.size() != 0) {
            env.texture_id = AssetId::from_logical_path(env.texture_path.view());
        }

        if (!env.texture_id.is_valid()) {
            release_one(env, assets);
            return;
        }

        if (env.texture_handle.is_valid() && env.resolved_texture_id == env.texture_id) {
            return;
        }

        release_one(env, assets);

        TextureAssetHandle loaded = assets.load_texture(env.texture_id);
        if (!loaded.is_valid()) {
            FR_LOG_ERR(
                "[EnvironmentSystem] Failed to resolve environment texture. path='{}', id={}",
                env.texture_path.view(), env.texture_id.value);
            return;
        }

        env.texture_handle = loaded;
        env.resolved_texture_id = env.texture_id;
    }

    static void release_one(EnvironmentPart &env, AssetManager &assets) noexcept {
        if (env.texture_handle.is_valid()) {
            assets.unload_texture(env.texture_handle);
        }

        env.texture_handle = {};
        env.resolved_texture_id = {};
    }
};

} // namespace fr
