/**
 * @file shader_compiler.cpp
 * @author Tfoedy
 * @brief Shader asset compiler.
 */

#include "shader_compiler.hpp"

#include "fr/asset/shader_format.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {
namespace {

[[nodiscard]] bool validate_shader_stages(const RawShader &shader,
                                          StringView output_path) noexcept {
    U32 vertex_count = 0;
    U32 fragment_count = 0;

    for (const RawShaderStage &stage : shader.stages) {
        if (stage.stage == CookedShaderStage::Vertex) {
            ++vertex_count;
        } else if (stage.stage == CookedShaderStage::Fragment) {
            ++fragment_count;
        } else {
            FR_LOG_ERR("[Cooker] Shader contains unsupported stage: {}", output_path);
            return false;
        }

        if (stage.source.size() == 0) {
            FR_LOG_ERR("[Cooker] Shader stage source is empty: {}", output_path);
            return false;
        }
    }

    if (vertex_count != 1 || fragment_count != 1) {
        FR_LOG_ERR("[Cooker] Shader requires exactly one vertex and one fragment stage: {}",
                   output_path);
        return false;
    }

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

} // namespace

bool compile_shader(const RawShader &shader, StringView output_path) noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot compile shader with empty output path.");
        return false;
    }

    if (shader.stages.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot compile shader with no stages: {}", output_path);
        return false;
    }

    if (!validate_shader_stages(shader, output_path)) {
        return false;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    DynamicArray<CookedShaderStageRecord> stage_records(alloc);
    DynamicArray<Byte> source_blob(alloc);

    stage_records.reserve(shader.stages.size());

    for (const RawShaderStage &stage : shader.stages) {
        CookedShaderStageRecord record{};
        record.stage = stage.stage;

        if (source_blob.size() > static_cast<USize>(0xFFFFFFFFu) ||
            stage.source.size() > static_cast<USize>(0xFFFFFFFFu) ||
            source_blob.size() > static_cast<USize>(0xFFFFFFFFu) - stage.source.size()) {
            FR_LOG_ERR("[Cooker] Shader source data is too large: {}", output_path);
            return false;
        }

        record.source_offset = static_cast<U32>(source_blob.size());
        record.source_size = static_cast<U32>(stage.source.size());

        append_bytes(source_blob, stage.source.data(), stage.source.size());
        stage_records.push_back(record);
    }

    if (stage_records.size() > static_cast<USize>(0xFFFFFFFFu) ||
        source_blob.size() > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Shader is too large for `.fshader` header: {}", output_path);
        return false;
    }

    const USize stage_table_size = stage_records.size() * sizeof(CookedShaderStageRecord);
    if (stage_table_size > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Shader stage table is too large: {}", output_path);
        return false;
    }

    const USize source_data_offset = sizeof(CookedShaderHeader) + stage_table_size;
    if (source_data_offset > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Shader source data offset is too large: {}", output_path);
        return false;
    }

    CookedShaderHeader header{};
    header.verify[0] = 'F';
    header.verify[1] = 'S';
    header.verify[2] = 'H';
    header.verify[3] = 'D';
    header.version = 1;

    header.stage_count = static_cast<U32>(stage_records.size());
    header.stage_table_offset = sizeof(CookedShaderHeader);

    header.stage_table_size = static_cast<U32>(stage_table_size);
    header.source_data_offset = static_cast<U32>(source_data_offset);

    header.source_data_size = static_cast<U32>(source_blob.size());

    DynamicArray<Byte> output(alloc);

    append_bytes(output, &header, sizeof(CookedShaderHeader));
    append_bytes(output, stage_records.data(),
                 stage_records.size() * sizeof(CookedShaderStageRecord));
    append_bytes(output, source_blob.data(), source_blob.size());

    String out_path = String::from_view(alloc, output_path);

    if (!file::ensure_parent_directory(out_path.view())) {
        FR_LOG_ERR("[Cooker] Failed to create cooked shader output directory: {}", output_path);
        return false;
    }

    if (!file::write_all_bytes(out_path, Slice<const Byte>(output.data(), output.size()))) {
        FR_LOG_ERR("[Cooker] Failed to write cooked shader: {}", output_path);
        return false;
    }

    FR_LOG("[Cooker] Wrote cooked shader: {}", output_path);
    return true;
}

} // namespace fr::asscooker
