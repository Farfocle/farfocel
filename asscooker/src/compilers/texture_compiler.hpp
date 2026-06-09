#pragma once
#include "formats/raw_texture.hpp"
#include "fr/core/string_view.hpp"

namespace fr::asscooker {
bool compile_texture(const RawTexture &raw_texture, StringView output_path);
}
