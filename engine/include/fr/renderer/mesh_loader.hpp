// THIS IS LIKELY A TEMPORARY CLASS
// IT WILL LIKELY BE REPLACED
#pragma once

#include "fr/core/string_view.hpp"
#include "fr/renderer/mesh.hpp"
#include "fr/renderer/render_device.hpp"

namespace fr {
[[nodiscard]] MeshData load_mesh_gltf(RenderDevice *device, StringView file_path);
}
