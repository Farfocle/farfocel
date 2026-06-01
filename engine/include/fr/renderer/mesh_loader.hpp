// WARNING: this is a temporary class that will be replaced by the game engine editor.
// there shouldn't be any code on the engine's side for directly loading meshes from a file path
// but for simplicity this is gonna stay until work is done on the editor
#pragma once

#include "fr/core/string_view.hpp"
#include "fr/renderer/mesh.hpp"
#include "fr/renderer/render_device.hpp"

namespace fr {
class AssetManager;
[[nodiscard]] MeshData load_mesh_gltf(AssetManager *ass, RenderDevice *device,
                                      StringView file_path);
} // namespace fr
