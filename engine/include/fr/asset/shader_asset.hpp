/**
 * @file shader_asset.hpp
 * @author Tfoedy
 * @brief Runtime decoding helpers for cooked shader assets.
 */

#pragma once

#include <type_traits>

#include "fr/core/alloc.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/shader_format.hpp"
#include "fr/logger/logger.hpp"

namespace fr {

/**
 * @brief Decoded shader source bundle.
 */
struct ShaderSourceBundle {
    String vertex;
    String fragment;

    explicit ShaderSourceBundle(Alloc *alloc) noexcept
        : vertex(alloc),
          fragment(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");
    }

    [[nodiscard]] bool is_valid() const noexcept {
        return vertex.size() != 0 && fragment.size() != 0;
    }

    void clear() noexcept {
        vertex.clear();
        fragment.clear();
    }
};

namespace impl {

template <typename T>
[[nodiscard]] inline bool read_cooked_shader_object(Slice<const Byte> bytes, USize offset,
                                                    T &out) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
        return false;
    }

    fr::mem::copy_raw_range(bytes.data() + offset, sizeof(T), reinterpret_cast<Byte *>(&out));
    return true;
}

[[nodiscard]] inline bool verify_cooked_shader_header(const CookedShaderHeader &header) noexcept {
    return header.verify[0] == 'F' && header.verify[1] == 'S' && header.verify[2] == 'H' &&
           header.verify[3] == 'D' && header.version == 1;
}

[[nodiscard]] inline bool validate_cooked_shader_range(USize file_size, U32 offset,
                                                       U32 size) noexcept {
    const USize range_offset = static_cast<USize>(offset);
    const USize range_size = static_cast<USize>(size);

    if (range_offset > file_size) {
        return false;
    }

    return range_size <= file_size - range_offset;
}

[[nodiscard]] inline bool
validate_cooked_shader_stage_range(const CookedShaderHeader &header,
                                   const CookedShaderStageRecord &record) noexcept {
    const USize source_offset = static_cast<USize>(record.source_offset);
    const USize source_size = static_cast<USize>(record.source_size);
    const USize source_data_size = static_cast<USize>(header.source_data_size);

    if (source_size == 0) {
        return false;
    }

    if (source_offset > source_data_size) {
        return false;
    }

    return source_size <= source_data_size - source_offset;
}

[[nodiscard]] inline bool read_cooked_shader_stage_source(Alloc *alloc, Slice<const Byte> bytes,
                                                          const CookedShaderHeader &header,
                                                          const CookedShaderStageRecord &record,
                                                          String &out) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (!validate_cooked_shader_stage_range(header, record)) {
        return false;
    }

    const USize source_block_offset = static_cast<USize>(header.source_data_offset);
    const USize stage_offset = source_block_offset + static_cast<USize>(record.source_offset);
    const USize stage_size = static_cast<USize>(record.source_size);

    if (stage_offset > bytes.size() || stage_size > bytes.size() - stage_offset) {
        return false;
    }

    out = String::from_sized_chars(
        alloc, reinterpret_cast<const char *>(bytes.data() + stage_offset), stage_size);

    return out.size() == stage_size;
}

} // namespace impl

/**
 * @brief Decodes .fshader bytes into shader sources.
 */
[[nodiscard]] inline bool load_cooked_shader_sources(Alloc *alloc, Slice<const Byte> bytes,
                                                     ShaderSourceBundle &out_sources) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    out_sources.clear();

    CookedShaderHeader header{};
    if (!impl::read_cooked_shader_object(bytes, 0, header)) {
        FR_LOG_ERR("Cooked shader is too small.");
        return false;
    }

    if (!impl::verify_cooked_shader_header(header)) {
        FR_LOG_ERR("Invalid cooked shader header.");
        return false;
    }

    if (header.stage_count == 0) {
        FR_LOG_ERR("Cooked shader has no stages.");
        return false;
    }

    if (!impl::validate_cooked_shader_range(bytes.size(), header.stage_table_offset,
                                            header.stage_table_size) ||
        !impl::validate_cooked_shader_range(bytes.size(), header.source_data_offset,
                                            header.source_data_size)) {
        FR_LOG_ERR("Cooked shader has invalid data ranges.");
        return false;
    }

    const USize expected_stage_table_size =
        static_cast<USize>(header.stage_count) * sizeof(CookedShaderStageRecord);

    if (static_cast<USize>(header.stage_table_size) != expected_stage_table_size) {
        FR_LOG_ERR("Cooked shader stage table size mismatch.");
        return false;
    }

    bool found_vertex = false;
    bool found_fragment = false;

    for (U32 i = 0; i < header.stage_count; ++i) {
        const USize record_offset = static_cast<USize>(header.stage_table_offset) +
                                    static_cast<USize>(i) * sizeof(CookedShaderStageRecord);

        CookedShaderStageRecord record{};
        if (!impl::read_cooked_shader_object(bytes, record_offset, record)) {
            FR_LOG_ERR("Failed to read cooked shader stage record.");
            return false;
        }

        switch (record.stage) {
        case CookedShaderStage::Vertex:
            if (found_vertex) {
                FR_LOG_ERR("Cooked shader has duplicate vertex stage.");
                return false;
            }

            if (!impl::read_cooked_shader_stage_source(alloc, bytes, header, record,
                                                       out_sources.vertex)) {
                FR_LOG_ERR("Failed to read vertex stage from cooked shader.");
                return false;
            }

            found_vertex = true;
            break;

        case CookedShaderStage::Fragment:
            if (found_fragment) {
                FR_LOG_ERR("Cooked shader has duplicate fragment stage.");
                return false;
            }

            if (!impl::read_cooked_shader_stage_source(alloc, bytes, header, record,
                                                       out_sources.fragment)) {
                FR_LOG_ERR("Failed to read fragment stage from cooked shader.");
                return false;
            }

            found_fragment = true;
            break;

        default:
            FR_LOG_ERR("Cooked shader contains unsupported stage.");
            return false;
        }
    }

    if (!found_vertex || !found_fragment) {
        FR_LOG_ERR("Cooked shader must contain vertex and fragment stages.");
        return false;
    }

    return out_sources.is_valid();
}

} // namespace fr
