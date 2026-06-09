#pragma once
#include "formats/raw_mesh.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {
bool compile_mesh(const RawMesh &mesh, StringView output_path);
}
