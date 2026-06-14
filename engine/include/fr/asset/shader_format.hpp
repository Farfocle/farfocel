/**
 * @file shader_format.hpp
 * @author Tfoedy
 * @brief Cooked shader asset format.
 */

#pragma once

#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Shader stage stored in .fshader.
 */
enum class CookedShaderStage : U32 {
    Vertex = 0,
    Fragment = 1,
};

#pragma pack(push, 1)

/**
 * @brief Header stored at the beginning of .fshader.
 */
struct CookedShaderHeader {
    char verify[4]{'F', 'S', 'H', 'D'};
    U32 version{1};

    U32 stage_count{0};
    U32 stage_table_offset{0};
    U32 stage_table_size{0};

    U32 source_data_offset{0};
    U32 source_data_size{0};
};

/**
 * @brief One shader stage entry in .fshader.
 */
struct CookedShaderStageRecord {
    CookedShaderStage stage{CookedShaderStage::Vertex};
    U32 source_offset{0};
    U32 source_size{0};
};

#pragma pack(pop)

} // namespace fr
