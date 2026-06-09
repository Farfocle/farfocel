#pragma once
#include "formats/raw_mesh.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {
bool import_gltf(StringView input_path, RawMesh &out_mesh);
}
