/**
 * @file mesh_compiler.hpp
 * @author Tfoedy
 * @brief Mesh asset compiler.
 */

#pragma once

#include "formats/raw_mesh.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {

/**
 * @brief Writes a raw mesh into .fmesh.
 */
bool compile_mesh(const RawMesh &mesh, StringView output_path) noexcept;

} // namespace fr::asscooker
