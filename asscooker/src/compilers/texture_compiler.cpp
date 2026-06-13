/**
 * @file texture_compiler.cpp
 * @author Tfoedy
 * @brief Texture asset compiler.
 */

#include "texture_compiler.hpp"

#include "fr/asset/asset_format.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {
namespace {

[[nodiscard]] bool checked_mul_u_size(USize a, USize b, USize &out) noexcept {
    if (a != 0 && b > static_cast<USize>(-1) / a) {
        return false;
    }

    out = a * b;
    return true;
}

[[nodiscard]] bool checked_add_u_size(USize a, USize b, USize &out) noexcept {
    if (b > static_cast<USize>(-1) - a) {
        return false;
    }

    out = a + b;
    return true;
}

void append_bytes(DynamicArray<Byte> &out, const void *data, USize size) noexcept {
    if (size == 0) {
        return;
    }

    FR_ASSERT(data, "source data must be non-null");

    const USize old_size = out.size();
    out.grow_default(old_size + size);

    fr::mem::copy_raw_range(reinterpret_cast<const Byte *>(data), size, out.data() + old_size);
}

[[nodiscard]] USize bytes_per_pixel_for_format(CookedTextureFormat format) noexcept {
    switch (format) {
    case CookedTextureFormat::RGBA8_UNORM:
    case CookedTextureFormat::RGBA8_SRGB:
        return 4;

    case CookedTextureFormat::RGBA32F_HDR:
        return sizeof(F32) * 4;

    default:
        return 0;
    }
}

[[nodiscard]] bool expected_texture_payload_size(const RawTexture &raw, USize &out_size) noexcept {
    const USize bytes_per_pixel = bytes_per_pixel_for_format(raw.format);
    if (bytes_per_pixel == 0) {
        return false;
    }

    USize pixel_count = 0;
    if (!checked_mul_u_size(static_cast<USize>(raw.width), static_cast<USize>(raw.height),
                            pixel_count)) {
        return false;
    }

    return checked_mul_u_size(pixel_count, bytes_per_pixel, out_size);
}

[[nodiscard]] bool validate_texture(const RawTexture &raw, StringView output_path) noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot compile texture with empty output path.");
        return false;
    }

    if (raw.width == 0 || raw.height == 0) {
        FR_LOG_ERR("[Cooker] Texture has invalid dimensions: {}x{}.", raw.width, raw.height);
        return false;
    }

    if (raw.mip_levels == 0) {
        FR_LOG_ERR("[Cooker] Texture has zero mip levels.");
        return false;
    }

    if (raw.pixels.is_empty()) {
        FR_LOG_ERR("[Cooker] Texture has empty pixel payload.");
        return false;
    }

    const USize expected_bytes_per_pixel = bytes_per_pixel_for_format(raw.format);
    if (expected_bytes_per_pixel == 0) {
        FR_LOG_ERR("[Cooker] Texture has unsupported format.");
        return false;
    }

    if (raw.bytes_per_pixel != expected_bytes_per_pixel) {
        FR_LOG_ERR("[Cooker] Texture bytes-per-pixel mismatch. Expected {}, got {}.",
                   expected_bytes_per_pixel, raw.bytes_per_pixel);
        return false;
    }

    if (raw.channels != 4) {
        FR_LOG_ERR("[Cooker] Texture must be stored as 4 channels, got {}.", raw.channels);
        return false;
    }

    USize expected_size = 0;
    if (!expected_texture_payload_size(raw, expected_size)) {
        FR_LOG_ERR("[Cooker] Texture payload size overflow.");
        return false;
    }

    if (raw.pixels.size() > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Texture payload is too large for `.ftex` header.");
        return false;
    }

    if (raw.pixels.size() != expected_size) {
        FR_LOG_ERR("[Cooker] Texture payload size mismatch. Expected {}, got {}.", expected_size,
                   raw.pixels.size());
        return false;
    }

    return true;
}

} // namespace

bool compile_texture(const RawTexture &raw, StringView output_path) noexcept {
    if (!validate_texture(raw, output_path)) {
        return false;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    CookedTextureHeader header{};
    header.base.verify[0] = 'F';
    header.base.verify[1] = 'T';
    header.base.verify[2] = 'E';
    header.base.verify[3] = 'X';
    header.base.version = 1;

    header.width = raw.width;
    header.height = raw.height;
    header.format = raw.format;
    header.image_data_size = static_cast<U32>(raw.pixels.size());
    header.mip_levels = raw.mip_levels;

    USize output_size = 0;
    if (!checked_add_u_size(sizeof(CookedTextureHeader), raw.pixels.size(), output_size)) {
        FR_LOG_ERR("[Cooker] Texture output size overflow: {}", output_path);
        return false;
    }

    DynamicArray<Byte> output(alloc);
    output.reserve(output_size);

    append_bytes(output, &header, sizeof(CookedTextureHeader));
    append_bytes(output, raw.pixels.data(), raw.pixels.size());

    String out_path = String::from_view(alloc, output_path);

    if (!file::write_all_bytes(out_path, Slice<const Byte>(output.data(), output.size()))) {
        FR_LOG_ERR("[Cooker] Failed to write cooked texture: {}", output_path);
        return false;
    }

    return true;
}

} // namespace fr::asscooker
