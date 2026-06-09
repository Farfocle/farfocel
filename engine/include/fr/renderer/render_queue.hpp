/**
 * @file render_queue.hpp
 * @author Tfoedy
 * @brief Per-frame render submission queue.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/renderer/render_light_data.hpp"
#include "fr/renderer/render_sort_key.hpp"

#include <algorithm>
#include <glm/glm.hpp>

namespace fr {
class RenderQueue {
public:
    explicit RenderQueue(Alloc *alloc = get_ambient_ctx().alloc) noexcept
        : m_alloc(alloc),
          m_packets(m_alloc),
          m_transforms(m_alloc),
          m_point_lights(m_alloc),
          m_spot_lights(m_alloc),
          m_directional_lights(m_alloc),
          m_point_shadows(m_alloc),
          m_spot_shadows(m_alloc) {
        m_packets.reserve(4096);
        m_transforms.reserve(4096);

        m_point_lights.reserve(256);
        m_spot_lights.reserve(MAX_SPOT_LIGHTS);
        m_directional_lights.reserve(4);

        m_point_shadows.reserve(MAX_SHADOWED_POINT_LIGHTS);
        m_spot_shadows.reserve(MAX_SHADOWED_SPOT_LIGHTS);
    }

    void clear_leftover() noexcept {
        m_packets.clear();
        m_transforms.clear();

        m_point_lights.clear();
        m_spot_lights.clear();
        m_directional_lights.clear();

        m_point_shadows.clear();
        m_spot_shadows.clear();
    }

    void send_call(const DrawCall &packet, const glm::mat4 &transform) noexcept {
        const U32 transform_index = static_cast<U32>(m_transforms.size());

        m_transforms.push_back(transform);

        DrawCall call = packet;
        call.transform_index = transform_index;

        m_packets.push_back(call);
    }

    void send_point_light(const PointLightData &light) noexcept {
        m_point_lights.push_back(light);
    }

    void send_spot_light(const SpotLightData &light) noexcept {
        m_spot_lights.push_back(light);
    }

    void send_directional_light(const DirectionalLightData &light) noexcept {
        m_directional_lights.push_back(light);
    }

    void send_point_shadow(const PointShadowData &shadow) noexcept {
        m_point_shadows.push_back(shadow);
    }

    void send_spot_shadow(const SpotShadowData &shadow) noexcept {
        m_spot_shadows.push_back(shadow);
    }

    void sort() noexcept {
        std::sort(m_packets.begin(), m_packets.end(), [](const DrawCall &a, const DrawCall &b) {
            const RenderPassType pass_a = a.key.pass_type();
            const RenderPassType pass_b = b.key.pass_type();

            if (pass_a != pass_b) {
                return a.key < b.key;
            }

            if (pass_a == RenderPassType::Transparent) {
                return a.sort_depth > b.sort_depth;
            }

            return a.key < b.key;
        });
    }

    [[nodiscard]] Slice<const DrawCall> get_calls() const noexcept {
        return m_packets.slice();
    }

    [[nodiscard]] Slice<const glm::mat4> get_transforms() const noexcept {
        return m_transforms.slice();
    }

    [[nodiscard]] Slice<const PointLightData> get_point_lights() const noexcept {
        return m_point_lights.slice();
    }

    [[nodiscard]] Slice<const SpotLightData> get_spot_lights() const noexcept {
        return m_spot_lights.slice();
    }

    [[nodiscard]] Slice<const DirectionalLightData> get_directional_lights() const noexcept {
        return m_directional_lights.slice();
    }

    [[nodiscard]] Slice<const PointShadowData> get_point_shadows() const noexcept {
        return m_point_shadows.slice();
    }

    [[nodiscard]] Slice<const SpotShadowData> get_spot_shadows() const noexcept {
        return m_spot_shadows.slice();
    }

    [[nodiscard]] bool is_empty() const noexcept {
        return m_packets.is_empty();
    }

private:
    Alloc *m_alloc{nullptr};

    DynamicArray<DrawCall> m_packets;
    DynamicArray<glm::mat4> m_transforms;

    DynamicArray<PointLightData> m_point_lights;
    DynamicArray<SpotLightData> m_spot_lights;
    DynamicArray<DirectionalLightData> m_directional_lights;

    DynamicArray<PointShadowData> m_point_shadows;
    DynamicArray<SpotShadowData> m_spot_shadows;
};

} // namespace fr
