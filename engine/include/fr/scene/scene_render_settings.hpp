/**
 * @file scene_render_settings.hpp
 * @brief Scene-level renderer settings stored as a world resource.
 */

#pragma once

#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/renderer/renderer_desc.hpp"
#include "fr/scene/render_scene_extractor.hpp"

namespace fr {

/**
 * @brief Persistent renderer settings stored as a world resource.
 * @note These settings are app-level (not scene-level) and are intentionally excluded
 * from scene serialization. They persist across scene loads.
 */
struct SceneRenderSettings {
    RenderLightingSettings lighting{};
    RenderAmbientOcclusionSettings ao{};
    RenderIblSettings ibl{};
    RenderDebugSettings debug{};
    RenderDirectionalShadowSettings directional_shadow_settings{};

    SceneRenderSettings() noexcept = default;

    template <typename Archive>
    void shape(Archive &archive) noexcept {
        archive.prop("lighting", lighting);
        archive.prop("ao", ao);
        archive.prop("ibl", ibl);
        archive.prop("debug", debug);
        archive.prop("directional_shadows", directional_shadow_settings);
    }
};

} // namespace fr

FR_TYPE(fr::SceneRenderSettings);
