/**
 * @file render_frame.hpp
 * @author Tfoedy
 * @brief Per-frame renderer submission data.
 */

#pragma once

#include "fr/core/algo.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_lights.hpp"
#include "fr/renderer/render_sort_key.hpp"

namespace fr {

/// @brief Material has a valid albedo texture.
constexpr U32 RENDER_MATERIAL_HAS_ALBEDO = 1u << 0;

/// @brief Material has a valid normal texture.
constexpr U32 RENDER_MATERIAL_HAS_NORMAL = 1u << 1;

/// @brief Material has a valid packed material texture.
constexpr U32 RENDER_MATERIAL_HAS_EXTRA = 1u << 2;

/**
 * @brief Frame-local material data resolved for one draw.
 */
struct RenderMaterialPacket {
    TextureHandle albedo{};
    TextureHandle normal{};
    TextureHandle extra{};

    Vec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};

    U32 shading_model{0};
    U32 blend_mode{0};
    U32 texture_flags{0};

    F32 alpha{1.0f};
    F32 alpha_cutoff{0.5f};
    F32 metallic_factor{0.0f};
    F32 roughness_factor{1.0f};
};

/**
 * @brief Indexed mesh draw submitted for one frame.
 */
struct alignas(8) DrawCall {
    RenderSortKey key{};

    RenderPipelineHandle pipe{};
    BufferHandle vbo{};
    BufferHandle ibo{};

    U32 index_count{0};
    U32 index_offset{0};
    U32 vertex_offset{0};
    U32 vbo_stride{0};

    U32 transform_index{0};
    U32 material_index{0};

    constexpr bool operator<(const DrawCall &other) const noexcept {
        return key < other.key;
    }
};

/**
 * @brief Draw lists grouped by renderer pass.
 */
struct RenderDrawLists {
    DynamicArray<DrawCall> opaque;
    DynamicArray<DrawCall> masked;
    DynamicArray<DrawCall> transparent;
    DynamicArray<DrawCall> shadow;

    explicit RenderDrawLists(Alloc *alloc) noexcept
        : opaque(alloc),
          masked(alloc),
          transparent(alloc),
          shadow(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");

        opaque.reserve(4096);
        masked.reserve(1024);
        transparent.reserve(1024);
        shadow.reserve(4096);
    }

    void clear() noexcept {
        opaque.clear();
        masked.clear();
        transparent.clear();
        shadow.clear();
    }

    void sort() noexcept {
        radix_sort_key(opaque.slice_mut(),
                       [](const DrawCall &call) noexcept -> U64 { return call.key.value; });

        radix_sort_key(masked.slice_mut(),
                       [](const DrawCall &call) noexcept -> U64 { return call.key.value; });

        /*
            Transparent depth is encoded inverted by the extractor, so regular ascending radix sort
            gives back-to-front ordering while still preserving pipeline/VBO/texture grouping in the
            high bits.
        */
        radix_sort_key(transparent.slice_mut(),
                       [](const DrawCall &call) noexcept -> U64 { return call.key.value; });

        radix_sort_key(shadow.slice_mut(),
                       [](const DrawCall &call) noexcept -> U64 { return call.key.value; });
    }
};

/**
 * @brief Renderer-facing frame submission.
 */
struct RenderFrameSubmission {
    Alloc *alloc{nullptr};

    DynamicArray<Mat4> transforms;
    DynamicArray<RenderMaterialPacket> materials;

    RenderDrawLists draws;

    DynamicArray<PointLightData> point_lights;
    DynamicArray<SpotLightData> spot_lights;
    DynamicArray<DirectionalLightData> directional_lights;

    DynamicArray<PointShadowData> point_shadows;
    DynamicArray<SpotShadowData> spot_shadows;

    explicit RenderFrameSubmission(Alloc *frame_alloc) noexcept
        : alloc(frame_alloc),
          transforms(frame_alloc),
          materials(frame_alloc),
          draws(frame_alloc),
          point_lights(frame_alloc),
          spot_lights(frame_alloc),
          directional_lights(frame_alloc),
          point_shadows(frame_alloc),
          spot_shadows(frame_alloc) {
        FR_ASSERT(frame_alloc, "allocator must be non-null");

        transforms.reserve(4096);
        materials.reserve(4096);

        point_lights.reserve(256);
        spot_lights.reserve(MAX_SPOT_LIGHTS);
        directional_lights.reserve(4);

        point_shadows.reserve(MAX_SHADOWED_POINT_LIGHTS);
        spot_shadows.reserve(MAX_SHADOWED_SPOT_LIGHTS);
    }

    void clear() noexcept {
        transforms.clear();
        materials.clear();
        draws.clear();

        point_lights.clear();
        spot_lights.clear();
        directional_lights.clear();

        point_shadows.clear();
        spot_shadows.clear();
    }

    void sort() noexcept {
        draws.sort();
    }

    [[nodiscard]] U32 push_transform(const Mat4 &transform) noexcept {
        const U32 index = static_cast<U32>(transforms.size());
        transforms.push_back(transform);
        return index;
    }

    [[nodiscard]] U32 push_material(const RenderMaterialPacket &material) noexcept {
        const U32 index = static_cast<U32>(materials.size());
        materials.push_back(material);
        return index;
    }

    void push_draw(RenderPass pass_type, const DrawCall &draw) noexcept {
        switch (pass_type) {
        case RenderPass::Opaque:
            draws.opaque.push_back(draw);
            return;

        case RenderPass::Masked:
            draws.masked.push_back(draw);
            return;

        case RenderPass::Transparent:
            draws.transparent.push_back(draw);
            return;

        default:
            FR_ASSERT(false, "unsupported draw pass type");
            return;
        }
    }

    void push_shadow_draw(const DrawCall &draw) noexcept {
        draws.shadow.push_back(draw);
    }
};

} // namespace fr
