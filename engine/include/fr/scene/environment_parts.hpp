/**
 * @file environment_parts.hpp
 * @author Tfoedy
 * @brief Scene environment asset parts.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_manager.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Scene-owned environment texture reference.
 *
 * @details
 * texture_path is persistent scene data and should point to a cooked logical .ftex path.
 * Runtime texture handles are owned by EnvironmentSystem and are intentionally not serialized.
 */
struct EnvironmentPart {
    String texture_path{};
    AssetId texture_id{};

    AssetId resolved_texture_id{};
    TextureAssetHandle texture_handle{};

    bool enabled{false};

    EnvironmentPart() noexcept = default;

    explicit EnvironmentPart(StringView path)
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

} // namespace fr

FR_TYPE(fr::EnvironmentPart);
