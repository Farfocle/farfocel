/**
 * @file app_ctx.hpp
 * @brief Lightweight non-owning app context stored as a world resource.
 */

#pragma once

#include "fr/asset/asset_manager.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/time.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/platform/window.hpp"
#include "fr/renderer/render_frame.hpp"
#include "fr/renderer/renderer.hpp"

namespace fr::scene {

/**
 * @brief Non-owning pointers to app infrastructure + per-frame transient state.
 * Stored as a world resource. Populated by setup helpers (e.g. setup_devtools_world).
 */
struct AppState {
    fr::Window *window{nullptr};
    fr::AssetManager *assets{nullptr};
    fr::AssetRegistry *registry{nullptr};
    fr::Alloc *alloc{nullptr};
    fr::Renderer *renderer{nullptr};
    fr::RenderFrameSubmission *submission{nullptr};

    fr::MouseButton camera_button{static_cast<fr::MouseButton>(3)}; // RMB
    fr::MouseButton pick_button{fr::MouseButton::Left};

    F32 dt{0.0f};
    F32 fps{0.0f};
    bool camera_active{false};

    S64 _last_tick{0};

    /// @brief Advances the frame timer. Call once per frame before world.run().
    void tick(F32 max_dt = 0.1f) noexcept {
        const S64 now = fr::time::get_steady_now_ms();
        if (_last_tick == 0) {
            _last_tick = now;
        }
        dt = static_cast<F32>(now - _last_tick) * 0.001f;
        if (dt > max_dt)
            dt = max_dt;
        _last_tick = now;
        fps = dt > 0.0f ? 1.0f / dt : 0.0f;
    }

    template <typename Archive>
    void shape(Archive &) noexcept {
    }
};

} // namespace fr::scene

FR_TYPE(fr::scene::AppState);
