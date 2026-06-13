/**
 * @file gltf_importer.cpp
 * @author Tfoedy
 * @brief glTF mesh importer.
 */

#include "gltf_importer.hpp"

#include "compilers/material_compiler.hpp"
#include "compilers/texture_compiler.hpp"
#include "formats/raw_material.hpp"
#include "formats/raw_texture.hpp"

#include "fr/asscooker/asscooker.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/math.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/thread_pool.hpp"
#include "fr/logger/logger.hpp"

#include <cgltf.h>
#include <mikktspace.h>
#include <stb_image.h>

#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace fr::asscooker {
namespace {

/**
 * @brief Texture baking task type.
 */
enum class TextureBakeTaskType {
    RegularTexture,
    MaterialExtraTexture,
};

/**
 * @brief Texture baking task queued by the glTF importer.
 */
struct TextureBakeTask {
    TextureBakeTaskType type{TextureBakeTaskType::RegularTexture};

    String in_path{};
    String secondary_path{};
    String out_path{};

    AssetId output_id{};

    bool is_srgb{false};
    bool needs_bake{true};
};

/**
 * @brief Result of one texture baking task.
 */
struct TextureBakeResult {
    bool ok{false};
};

/**
 * @brief Imported material cache entry.
 */
struct ImportedMaterialRecord {
    const cgltf_material *source{nullptr};
    AssetId material_id{};
    String material_path{};
};

/**
 * @brief User data passed to MikkTSpace callbacks.
 */
struct MikkUserData {
    RawMesh *mesh{nullptr};
    RawSubMesh *submesh{nullptr};
};

/**
 * @brief Tracks already emitted path warnings for a single import.
 */
struct PathWarningState {
    bool gltf_base_dir_absolute{false};
    bool material_output_absolute{false};
    bool texture_output_absolute{false};
    bool material_extra_output_absolute{false};

    bool gltf_base_dir_parent{false};
    bool material_output_parent{false};
    bool texture_output_parent{false};
    bool material_extra_output_parent{false};
};

static PathWarningState g_path_warning_state{};

static bool is_path_separator(char c) noexcept {
    return c == '/' || c == '\\';
}

static bool is_absolute_path(StringView path) noexcept {
    if (path.is_empty()) {
        return false;
    }

    if (path[0] == '/' || path[0] == '\\') {
        return true;
    }

    if (path.size() >= 3 && path[1] == ':' && is_path_separator(path[2])) {
        const char drive = path[0];
        return (drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z');
    }

    return false;
}

static void append_decimal(String &out, USize value) noexcept {
    char buffer[32]{};
    USize count = 0;

    do {
        buffer[count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value != 0);

    while (count > 0) {
        out.push_back(buffer[--count]);
    }
}

static U32 mip_count_for_size(S32 width, S32 height) noexcept {
    S32 max_dim = fr::math::max(width, height);
    U32 mip_count = 1;

    while (max_dim > 1) {
        max_dim /= 2;
        ++mip_count;
    }

    return mip_count;
}

/**
 * @brief Normalizes a path to a slash-separated representation.
 */
static String normalize_slash_path(StringView path) noexcept {
    String input = file::get_normalized_unix(path);
    String out = String::with_capacity(input.size());

    bool last_was_slash = false;

    for (USize i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (c == '/') {
            if (out.size() == 0) {
                out.push_back(c);
                last_was_slash = true;
                continue;
            }

            if (last_was_slash) {
                continue;
            }

            out.push_back(c);
            last_was_slash = true;
            continue;
        }

        out.push_back(c);
        last_was_slash = false;
    }

    while (out.starts_with("./")) {
        out.erase(0, 2);
    }

    return out;
}

static String join_paths(StringView base, StringView relative) noexcept {
    if (base.is_empty()) {
        return normalize_slash_path(relative);
    }

    if (relative.is_empty()) {
        return normalize_slash_path(base);
    }

    if (is_absolute_path(relative)) {
        return normalize_slash_path(relative);
    }

    String out = String::with_capacity(base.size() + relative.size() + 1);
    out.append(base);

    if (!is_path_separator(out.back())) {
        out.push_back('/');
    }

    out.append(relative);

    return normalize_slash_path(out.view());
}

static String replace_extension(StringView path, StringView extension) noexcept {
    String normalized = normalize_slash_path(path);

    StringView filename = file::get_filename(normalized.view());
    const USize filename_offset = filename.data() - normalized.data();

    USize dot_pos = String::npos;

    for (USize i = filename.size(); i-- > 0;) {
        if (filename[i] == '.') {
            if (i != 0) {
                dot_pos = filename_offset + i;
            }

            break;
        }
    }

    if (dot_pos == String::npos) {
        normalized.append(extension);
        return normalized;
    }

    normalized.shrink(dot_pos);
    normalized.append(extension);
    return normalized;
}

static String replace_filename(StringView path, StringView new_filename) noexcept {
    String normalized = normalize_slash_path(path);
    StringView parent = file::get_parent_path(normalized.view());

    if (parent.is_empty()) {
        return String::from_view(new_filename);
    }

    return join_paths(parent, new_filename);
}

static AssetId asset_id_from_logical_path(StringView path) noexcept {
    return AssetId::from_logical_path(path);
}

static bool has_cooked_asset_output(const DynamicArray<CookedAssetOutput> *outputs,
                                    AssetId id) noexcept {
    if (!outputs || !id.is_valid()) {
        return false;
    }

    for (USize i = 0; i < outputs->size(); ++i) {
        if ((*outputs)[i].id == id) {
            return true;
        }
    }

    return false;
}

static void append_cooked_asset_output_once(DynamicArray<CookedAssetOutput> *outputs, AssetId id,
                                            AssetKind kind, StringView path) noexcept {
    if (!outputs || has_cooked_asset_output(outputs, id)) {
        return;
    }

    append_cooked_asset_output(outputs, id, kind, path);
}

static bool *absolute_warning_flag(StringView label) noexcept {
    if (label == StringView("glTF base directory")) {
        return &g_path_warning_state.gltf_base_dir_absolute;
    }

    if (label == StringView("material cooked output")) {
        return &g_path_warning_state.material_output_absolute;
    }

    if (label == StringView("texture cooked output")) {
        return &g_path_warning_state.texture_output_absolute;
    }

    if (label == StringView("material extra texture output")) {
        return &g_path_warning_state.material_extra_output_absolute;
    }

    return nullptr;
}

static bool *parent_warning_flag(StringView label) noexcept {
    if (label == StringView("glTF base directory")) {
        return &g_path_warning_state.gltf_base_dir_parent;
    }

    if (label == StringView("material cooked output")) {
        return &g_path_warning_state.material_output_parent;
    }

    if (label == StringView("texture cooked output")) {
        return &g_path_warning_state.texture_output_parent;
    }

    if (label == StringView("material extra texture output")) {
        return &g_path_warning_state.material_extra_output_parent;
    }

    return nullptr;
}

static void warn_if_unstable_logical_path(StringView path, StringView label) noexcept {
    if (is_absolute_path(path)) {
        bool *flag = absolute_warning_flag(label);
        if (!flag || !*flag) {
            FR_LOG_WARN("[Cooker] {} path is absolute. AssetIds based on absolute paths are not "
                        "portable: {}",
                        label, path);

            if (flag) {
                *flag = true;
            }
        }
    }

    String normalized = normalize_slash_path(path);
    if (normalized.contains("../")) {
        bool *flag = parent_warning_flag(label);
        if (!flag || !*flag) {
            FR_LOG_WARN("[Cooker] {} path contains '..'. Prefer paths relative to asset root: {}",
                        label, path);

            if (flag) {
                *flag = true;
            }
        }
    }
}

static void log_texture_decode_failure(StringView path, const char *label) noexcept {
    String path_str = String::from_view(path);
    const bool exists = file::exists(path_str);
    const char *reason = stbi_failure_reason();

    FR_LOG_ERR("[Cooker] Failed to load {}.\n  path:   {}\n  exists: {}\n  stb:    {}",
               label ? label : "texture", path, exists ? "yes" : "no", reason ? reason : "unknown");
}

static bool has_pending_texture_task(const DynamicArray<TextureBakeTask> &tasks,
                                     const String &output_path) noexcept {
    for (USize i = 0; i < tasks.size(); ++i) {
        if (tasks[i].out_path == output_path) {
            return true;
        }
    }

    return false;
}

static void queue_regular_texture(DynamicArray<TextureBakeTask> &tasks, const String &input_path,
                                  const String &output_path, bool is_srgb, bool force) {
    if (input_path.size() == 0 || output_path.size() == 0) {
        return;
    }

    if (has_pending_texture_task(tasks, output_path)) {
        return;
    }

    TextureBakeTask task{};
    task.type = TextureBakeTaskType::RegularTexture;
    task.in_path = String::from_view(input_path.view());
    task.out_path = String::from_view(output_path.view());
    task.output_id = asset_id_from_logical_path(output_path.view());
    task.is_srgb = is_srgb;
    task.needs_bake = force || !file::exists(output_path);

    tasks.push_back(std::move(task));
}

static void queue_material_extra_texture(DynamicArray<TextureBakeTask> &tasks,
                                         const String &metallic_roughness_path,
                                         const String &occlusion_path, const String &output_path,
                                         bool force) {
    if (output_path.size() == 0) {
        return;
    }

    if (has_pending_texture_task(tasks, output_path)) {
        return;
    }

    TextureBakeTask task{};
    task.type = TextureBakeTaskType::MaterialExtraTexture;
    task.in_path = String::from_view(metallic_roughness_path.view());
    task.secondary_path = String::from_view(occlusion_path.view());
    task.out_path = String::from_view(output_path.view());
    task.output_id = asset_id_from_logical_path(output_path.view());
    task.is_srgb = false;
    task.needs_bake = force || !file::exists(output_path);

    tasks.push_back(std::move(task));
}

static String get_source_texture_path(const cgltf_texture_view &view, StringView base_dir) {
    if (!view.texture || !view.texture->image || !view.texture->image->uri) {
        return String::from_chars("");
    }

    return join_paths(base_dir, StringView(view.texture->image->uri));
}

static String make_regular_ftex_path(const cgltf_texture_view &view, StringView base_dir) {
    if (!view.texture || !view.texture->image || !view.texture->image->uri) {
        return String::from_chars("");
    }

    String source_path = join_paths(base_dir, StringView(view.texture->image->uri));
    String cooked_path = replace_extension(source_path.view(), ".ftex");

    warn_if_unstable_logical_path(cooked_path.view(), "texture cooked output");

    return cooked_path;
}

static String make_extra_ftex_path(const cgltf_material &material, StringView base_dir) {
    const cgltf_texture_view *source_view = nullptr;

    if (material.has_pbr_metallic_roughness &&
        material.pbr_metallic_roughness.metallic_roughness_texture.texture &&
        material.pbr_metallic_roughness.metallic_roughness_texture.texture->image &&
        material.pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri) {
        source_view = &material.pbr_metallic_roughness.metallic_roughness_texture;
    } else if (material.occlusion_texture.texture && material.occlusion_texture.texture->image &&
               material.occlusion_texture.texture->image->uri) {
        source_view = &material.occlusion_texture;
    }

    if (!source_view) {
        return String::from_chars("");
    }

    String source_path = join_paths(base_dir, StringView(source_view->texture->image->uri));
    StringView stem = file::get_stem(source_path.view());

    String filename = String::with_capacity(stem.size() + 12);
    filename.append(stem);
    filename.append("_extra.ftex");

    String cooked_path = replace_filename(source_path.view(), filename.view());

    warn_if_unstable_logical_path(cooked_path.view(), "material extra texture output");

    return cooked_path;
}

static String make_material_fmat_path(const cgltf_material &material, StringView base_dir,
                                      USize material_index) {
    String name =
        material.name ? String::from_chars(material.name) : String::from_chars("material");

    if (name.size() == 0) {
        name = String::from_chars("material");
    }

    for (USize i = 0; i < name.size(); ++i) {
        char &c = name[i];

        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }

    String filename = String::with_capacity(name.size() + 32);
    filename.append(name.view());
    filename.push_back('_');
    append_decimal(filename, material_index);
    filename.append(".fmat");

    String cooked_path = join_paths(base_dir, filename.view());

    warn_if_unstable_logical_path(cooked_path.view(), "material cooked output");

    return cooked_path;
}

static USize find_material_index(const cgltf_data *data, const cgltf_material *material) noexcept {
    if (!data || !material || !data->materials) {
        return 0;
    }

    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        if (&data->materials[i] == material) {
            return static_cast<USize>(i);
        }
    }

    return 0;
}

static bool find_imported_material(const DynamicArray<ImportedMaterialRecord> &materials,
                                   const cgltf_material *source,
                                   AssetId &out_material_id) noexcept {
    if (!source) {
        return false;
    }

    for (USize i = 0; i < materials.size(); ++i) {
        if (materials[i].source == source) {
            out_material_id = materials[i].material_id;
            return true;
        }
    }

    return false;
}

static String resolve_regular_texture(const cgltf_texture_view &view, StringView base_dir,
                                      DynamicArray<TextureBakeTask> &tasks, bool is_srgb,
                                      bool force) {
    String input_path = get_source_texture_path(view, base_dir);
    String output_path = make_regular_ftex_path(view, base_dir);

    queue_regular_texture(tasks, input_path, output_path, is_srgb, force);

    return output_path;
}

static String resolve_material_extra_texture(const cgltf_material &material, StringView base_dir,
                                             DynamicArray<TextureBakeTask> &tasks, bool force) {
    bool has_metallic_roughness = false;
    bool has_occlusion = false;

    cgltf_texture_view metallic_roughness_view{};
    cgltf_texture_view occlusion_view{};

    if (material.has_pbr_metallic_roughness &&
        material.pbr_metallic_roughness.metallic_roughness_texture.texture &&
        material.pbr_metallic_roughness.metallic_roughness_texture.texture->image &&
        material.pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri) {
        metallic_roughness_view = material.pbr_metallic_roughness.metallic_roughness_texture;
        has_metallic_roughness = true;
    }

    if (material.occlusion_texture.texture && material.occlusion_texture.texture->image &&
        material.occlusion_texture.texture->image->uri) {
        occlusion_view = material.occlusion_texture;
        has_occlusion = true;
    }

    if (!has_metallic_roughness && !has_occlusion) {
        return String::from_chars("");
    }

    String metallic_roughness_path =
        has_metallic_roughness ? get_source_texture_path(metallic_roughness_view, base_dir)
                               : String::from_chars("");

    String occlusion_path =
        has_occlusion ? get_source_texture_path(occlusion_view, base_dir) : String::from_chars("");

    String output_path = make_extra_ftex_path(material, base_dir);

    queue_material_extra_texture(tasks, metallic_roughness_path, occlusion_path, output_path,
                                 force);

    return output_path;
}

static bool cook_gltf_material_asset(const cgltf_data *data, const cgltf_material &material,
                                     StringView base_dir,
                                     DynamicArray<TextureBakeTask> &texture_tasks,
                                     DynamicArray<CookedAssetOutput> *outputs, bool force,
                                     String &out_material_path, AssetId &out_material_id) {
    const USize material_index = find_material_index(data, &material);

    out_material_path = make_material_fmat_path(material, base_dir, material_index);
    out_material_id = asset_id_from_logical_path(out_material_path.view());

    String albedo_path;
    String normal_path;
    String extra_path;

    RawMaterial raw{};

    static_assert(sizeof(raw.base_color_factor) >= sizeof(F32) * 4,
                  "RawMaterial::base_color_factor must contain 4 floats.");

    if (material.has_pbr_metallic_roughness) {
        albedo_path = resolve_regular_texture(material.pbr_metallic_roughness.base_color_texture,
                                              base_dir, texture_tasks, true, force);

        raw.base_color_factor[0] = material.pbr_metallic_roughness.base_color_factor[0];
        raw.base_color_factor[1] = material.pbr_metallic_roughness.base_color_factor[1];
        raw.base_color_factor[2] = material.pbr_metallic_roughness.base_color_factor[2];
        raw.base_color_factor[3] = material.pbr_metallic_roughness.base_color_factor[3];

        raw.metallic_factor = material.pbr_metallic_roughness.metallic_factor;
        raw.roughness_factor = material.pbr_metallic_roughness.roughness_factor;
    }

    normal_path =
        resolve_regular_texture(material.normal_texture, base_dir, texture_tasks, false, force);
    extra_path = resolve_material_extra_texture(material, base_dir, texture_tasks, force);

    if (albedo_path.size() > 0) {
        raw.albedo_texture = asset_id_from_logical_path(albedo_path.view());
    }

    if (normal_path.size() > 0) {
        raw.normal_texture = asset_id_from_logical_path(normal_path.view());
    }

    if (extra_path.size() > 0) {
        raw.extra_texture = asset_id_from_logical_path(extra_path.view());
    }

    raw.shading_model = MaterialShadingModel::PBR;

    if (material.alpha_mode == cgltf_alpha_mode_mask) {
        raw.blend_mode = MaterialBlendMode::Masked;
        raw.alpha_cutoff = material.alpha_cutoff;
    } else if (material.alpha_mode == cgltf_alpha_mode_blend) {
        raw.blend_mode = MaterialBlendMode::Transparent;
    } else {
        raw.blend_mode = MaterialBlendMode::Opaque;
    }

    raw.alpha = raw.base_color_factor[3];

    const bool needs_compile = force || !file::exists(out_material_path);
    if (needs_compile) {
        if (!compile_material(raw, out_material_path.view())) {
            return false;
        }
    }

    append_cooked_asset_output_once(outputs, out_material_id, AssetKind::Material,
                                    out_material_path.view());

    return true;
}

/**
 * @brief Bakes a renderer-specific material extra texture.
 *
 * @details
 * Input:
 *
 * - glTF metallic-roughness texture:
 *   - G = roughness
 *   - B = metallic
 *
 * - glTF occlusion texture:
 *   - R = ambient occlusion
 *
 * Output:
 *
 * - R = metallic
 * - G = roughness
 * - B = ambient occlusion
 * - A = 255
 */
static bool bake_material_extra_texture(StringView metallic_roughness_path,
                                        StringView occlusion_path, StringView output_path) {
    String mr_path = String::from_view(metallic_roughness_path);
    String ao_path = String::from_view(occlusion_path);

    const bool has_metallic_roughness = mr_path.size() > 0;
    const bool has_occlusion = ao_path.size() > 0;

    if (!has_metallic_roughness && !has_occlusion) {
        return false;
    }

    int mr_width = 0;
    int mr_height = 0;
    int mr_channels = 0;

    int ao_width = 0;
    int ao_height = 0;
    int ao_channels = 0;

    stbi_uc *mr_pixels = nullptr;
    stbi_uc *ao_pixels = nullptr;

    if (has_metallic_roughness) {
        mr_pixels = stbi_load(mr_path.c_str(), &mr_width, &mr_height, &mr_channels, 4);
        if (!mr_pixels) {
            log_texture_decode_failure(mr_path.view(), "metallic-roughness texture");
            return false;
        }
    }

    if (has_occlusion) {
        ao_pixels = stbi_load(ao_path.c_str(), &ao_width, &ao_height, &ao_channels, 4);
        if (!ao_pixels) {
            log_texture_decode_failure(ao_path.view(), "occlusion texture");

            if (mr_pixels) {
                stbi_image_free(mr_pixels);
            }

            return false;
        }
    }

    const int width = has_metallic_roughness ? mr_width : ao_width;
    const int height = has_metallic_roughness ? mr_height : ao_height;

    if (width <= 0 || height <= 0) {
        FR_LOG_ERR("[Cooker] Invalid material extra texture dimensions. Width: {}, height: {}.",
                   width, height);

        if (mr_pixels) {
            stbi_image_free(mr_pixels);
        }

        if (ao_pixels) {
            stbi_image_free(ao_pixels);
        }

        return false;
    }

    if (has_metallic_roughness && has_occlusion &&
        (mr_width != ao_width || mr_height != ao_height)) {
        FR_LOG_ERR("[Cooker] Metallic-roughness and occlusion textures have different sizes. "
                   "Metallic-roughness: {}x{}, occlusion: {}x{}.",
                   mr_width, mr_height, ao_width, ao_height);

        stbi_image_free(mr_pixels);
        stbi_image_free(ao_pixels);
        return false;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    RawTexture raw(alloc);
    raw.width = static_cast<U32>(width);
    raw.height = static_cast<U32>(height);
    raw.channels = 4;
    raw.bytes_per_pixel = 4;
    raw.format = CookedTextureFormat::RGBA8_UNORM;
    raw.mip_levels = mip_count_for_size(width, height);

    const USize pixel_count = static_cast<USize>(width) * static_cast<USize>(height);
    raw.pixels.grow_default(pixel_count * 4);

    for (USize pixel = 0; pixel < pixel_count; ++pixel) {
        const USize src = pixel * 4;
        const USize dst = pixel * 4;

        U8 metallic = 0;
        U8 roughness = 255;
        U8 occlusion = 255;

        if (mr_pixels) {
            roughness = mr_pixels[src + 1];
            metallic = mr_pixels[src + 2];
        }

        if (ao_pixels) {
            occlusion = ao_pixels[src + 0];
        }

        raw.pixels[dst + 0] = metallic;
        raw.pixels[dst + 1] = roughness;
        raw.pixels[dst + 2] = occlusion;
        raw.pixels[dst + 3] = 255;
    }

    if (mr_pixels) {
        stbi_image_free(mr_pixels);
    }

    if (ao_pixels) {
        stbi_image_free(ao_pixels);
    }

    return compile_texture(raw, output_path);
}

static const cgltf_accessor *find_attribute(const cgltf_primitive &primitive,
                                            cgltf_attribute_type type) {
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
        const cgltf_attribute &attribute = primitive.attributes[i];

        if (attribute.type == type) {
            return attribute.data;
        }
    }

    return nullptr;
}

static void write_matrix(const glm::mat4 &matrix, F32 (&out_matrix)[16]) noexcept {
    const F32 *values = glm::value_ptr(matrix);

    for (U32 i = 0; i < 16; ++i) {
        out_matrix[i] = values[i];
    }
}

static glm::mat4 get_node_local_transform(const cgltf_node *node) {
    if (!node) {
        return glm::mat4(1.0f);
    }

    if (node->has_matrix) {
        return glm::make_mat4(node->matrix);
    }

    glm::vec3 translation(0.0f);
    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale(1.0f);

    if (node->has_translation) {
        translation = glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
    }

    if (node->has_rotation) {
        rotation =
            glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
    }

    if (node->has_scale) {
        scale = glm::vec3(node->scale[0], node->scale[1], node->scale[2]);
    }

    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) *
           glm::scale(glm::mat4(1.0f), scale);
}

static void reset_aabb(F32 (&min_extents)[3], F32 (&max_extents)[3]) noexcept {
    min_extents[0] = 1.0e30f;
    min_extents[1] = 1.0e30f;
    min_extents[2] = 1.0e30f;

    max_extents[0] = -1.0e30f;
    max_extents[1] = -1.0e30f;
    max_extents[2] = -1.0e30f;
}

static void expand_aabb(F32 (&min_extents)[3], F32 (&max_extents)[3],
                        const F32 (&position)[3]) noexcept {
    for (U32 i = 0; i < 3; ++i) {
        min_extents[i] = fr::math::min(min_extents[i], position[i]);
        max_extents[i] = fr::math::max(max_extents[i], position[i]);
    }
}

static void finalize_aabb(F32 (&min_extents)[3], F32 (&max_extents)[3]) noexcept {
    if (min_extents[0] <= max_extents[0]) {
        return;
    }

    min_extents[0] = 0.0f;
    min_extents[1] = 0.0f;
    min_extents[2] = 0.0f;

    max_extents[0] = 0.0f;
    max_extents[1] = 0.0f;
    max_extents[2] = 0.0f;
}

static int mikk_get_num_faces(const SMikkTSpaceContext *context) {
    const MikkUserData *data = static_cast<const MikkUserData *>(context->m_pUserData);
    return static_cast<int>(data->submesh->index_count / 3);
}

static int mikk_get_num_vertices_of_face(const SMikkTSpaceContext *, int) {
    return 3;
}

static void mikk_get_position(const SMikkTSpaceContext *context, float out_position[], int face,
                              int vertex) {
    MikkUserData *data = static_cast<MikkUserData *>(context->m_pUserData);

    const U32 local_index =
        data->mesh->indices[data->submesh->index_offset + static_cast<U32>(face * 3 + vertex)];

    const RawVertex &raw_vertex = data->mesh->vertices[data->submesh->vertex_offset + local_index];

    out_position[0] = raw_vertex.position[0];
    out_position[1] = raw_vertex.position[1];
    out_position[2] = raw_vertex.position[2];
}

static void mikk_get_normal(const SMikkTSpaceContext *context, float out_normal[], int face,
                            int vertex) {
    MikkUserData *data = static_cast<MikkUserData *>(context->m_pUserData);

    const U32 local_index =
        data->mesh->indices[data->submesh->index_offset + static_cast<U32>(face * 3 + vertex)];

    const RawVertex &raw_vertex = data->mesh->vertices[data->submesh->vertex_offset + local_index];

    out_normal[0] = raw_vertex.normal[0];
    out_normal[1] = raw_vertex.normal[1];
    out_normal[2] = raw_vertex.normal[2];
}

static void mikk_get_tex_coord(const SMikkTSpaceContext *context, float out_tex_coord[], int face,
                               int vertex) {
    MikkUserData *data = static_cast<MikkUserData *>(context->m_pUserData);

    const U32 local_index =
        data->mesh->indices[data->submesh->index_offset + static_cast<U32>(face * 3 + vertex)];

    const RawVertex &raw_vertex = data->mesh->vertices[data->submesh->vertex_offset + local_index];

    out_tex_coord[0] = raw_vertex.uv[0];
    out_tex_coord[1] = raw_vertex.uv[1];
}

static void mikk_set_tspace_basic(const SMikkTSpaceContext *context, const float tangent[],
                                  float sign, int face, int vertex) {
    MikkUserData *data = static_cast<MikkUserData *>(context->m_pUserData);

    const U32 local_index =
        data->mesh->indices[data->submesh->index_offset + static_cast<U32>(face * 3 + vertex)];

    RawVertex &raw_vertex = data->mesh->vertices[data->submesh->vertex_offset + local_index];

    raw_vertex.tangent[0] += tangent[0];
    raw_vertex.tangent[1] += tangent[1];
    raw_vertex.tangent[2] += tangent[2];
    raw_vertex.tangent[3] = sign;
}

static void generate_missing_tangents(RawMesh &mesh, RawSubMesh &submesh, U32 vertex_count) {
    for (U32 i = 0; i < vertex_count; ++i) {
        RawVertex &vertex = mesh.vertices[submesh.vertex_offset + i];

        vertex.tangent[0] = 0.0f;
        vertex.tangent[1] = 0.0f;
        vertex.tangent[2] = 0.0f;
        vertex.tangent[3] = 1.0f;
    }

    SMikkTSpaceInterface interface_data{};
    interface_data.m_getNumFaces = mikk_get_num_faces;
    interface_data.m_getNumVerticesOfFace = mikk_get_num_vertices_of_face;
    interface_data.m_getPosition = mikk_get_position;
    interface_data.m_getNormal = mikk_get_normal;
    interface_data.m_getTexCoord = mikk_get_tex_coord;
    interface_data.m_setTSpaceBasic = mikk_set_tspace_basic;

    MikkUserData user_data{};
    user_data.mesh = &mesh;
    user_data.submesh = &submesh;

    SMikkTSpaceContext context{};
    context.m_pInterface = &interface_data;
    context.m_pUserData = &user_data;

    const bool generated = genTangSpaceDefault(&context) != 0;

    for (U32 i = 0; i < vertex_count; ++i) {
        RawVertex &vertex = mesh.vertices[submesh.vertex_offset + i];

        glm::vec3 tangent(vertex.tangent[0], vertex.tangent[1], vertex.tangent[2]);

        if (generated && glm::length(tangent) > 0.0001f) {
            tangent = glm::normalize(tangent);

            vertex.tangent[0] = tangent.x;
            vertex.tangent[1] = tangent.y;
            vertex.tangent[2] = tangent.z;

            if (vertex.tangent[3] == 0.0f) {
                vertex.tangent[3] = 1.0f;
            }

            continue;
        }

        vertex.tangent[0] = 1.0f;
        vertex.tangent[1] = 0.0f;
        vertex.tangent[2] = 0.0f;
        vertex.tangent[3] = 1.0f;
    }
}

static bool validate_primitive_index_range(const RawMesh &mesh,
                                           const RawSubMesh &submesh) noexcept {
    const USize vertex_offset = static_cast<USize>(submesh.vertex_offset);

    if (vertex_offset >= mesh.vertices.size()) {
        FR_LOG_ERR("[Cooker] glTF primitive vertex offset is out of bounds.");
        return false;
    }

    const USize local_vertex_count = mesh.vertices.size() - vertex_offset;
    if (local_vertex_count == 0) {
        FR_LOG_ERR("[Cooker] glTF primitive has no imported local vertices.");
        return false;
    }

    const USize index_begin = static_cast<USize>(submesh.index_offset);
    const USize index_count = static_cast<USize>(submesh.index_count);

    if (index_begin >= mesh.indices.size()) {
        FR_LOG_ERR("[Cooker] glTF primitive index offset is out of bounds.");
        return false;
    }

    if (index_count > mesh.indices.size() - index_begin) {
        FR_LOG_ERR("[Cooker] glTF primitive index range is out of bounds.");
        return false;
    }

    const USize index_end = index_begin + index_count;

    for (USize i = index_begin; i < index_end; ++i) {
        const USize local_index = static_cast<USize>(mesh.indices[i]);

        if (local_index >= local_vertex_count) {
            FR_LOG_ERR("[Cooker] glTF primitive references vertex out of bounds: {} >= {}.",
                       local_index, local_vertex_count);
            return false;
        }
    }

    return true;
}

static bool import_primitive(const cgltf_data *data, const cgltf_primitive &primitive,
                             const glm::mat4 &node_transform, StringView base_dir,
                             DynamicArray<TextureBakeTask> &texture_tasks,
                             DynamicArray<ImportedMaterialRecord> &imported_materials,
                             DynamicArray<CookedAssetOutput> *outputs, bool force,
                             RawMesh &out_mesh) {
    if (primitive.type != cgltf_primitive_type_triangles) {
        FR_LOG_WARN("[Cooker] Skipping non-triangle glTF primitive.");
        return true;
    }

    const cgltf_accessor *position_accessor =
        find_attribute(primitive, cgltf_attribute_type_position);

    if (!position_accessor || position_accessor->count == 0) {
        return true;
    }

    if (position_accessor->count > static_cast<cgltf_size>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] glTF primitive has too many vertices.");
        return false;
    }

    if (primitive.indices && primitive.indices->count > static_cast<cgltf_size>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] glTF primitive has too many indices.");
        return false;
    }

    if (out_mesh.vertices.size() > static_cast<USize>(0xFFFFFFFFu) ||
        out_mesh.indices.size() > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Imported mesh is too large.");
        return false;
    }

    const U32 vertex_count = static_cast<U32>(position_accessor->count);
    const U32 primitive_index_count =
        primitive.indices ? static_cast<U32>(primitive.indices->count) : vertex_count;
    (void)primitive_index_count;

    const cgltf_accessor *normal_accessor = find_attribute(primitive, cgltf_attribute_type_normal);
    const cgltf_accessor *uv_accessor = find_attribute(primitive, cgltf_attribute_type_texcoord);
    const cgltf_accessor *tangent_accessor =
        find_attribute(primitive, cgltf_attribute_type_tangent);

    RawSubMesh submesh{};
    submesh.vertex_offset = static_cast<U32>(out_mesh.vertices.size());
    submesh.index_offset = static_cast<U32>(out_mesh.indices.size());
    submesh.pass_type = 0;

    write_matrix(node_transform, submesh.transform);
    reset_aabb(submesh.aabb_min, submesh.aabb_max);

    if (static_cast<USize>(vertex_count) > static_cast<USize>(-1) - out_mesh.vertices.size()) {
        FR_LOG_ERR("[Cooker] Imported mesh vertex count overflow.");
        return false;
    }

    out_mesh.vertices.reserve(out_mesh.vertices.size() + static_cast<USize>(vertex_count));

    for (U32 i = 0; i < vertex_count; ++i) {
        RawVertex vertex{};

        float position[3] = {0.0f, 0.0f, 0.0f};
        if (!cgltf_accessor_read_float(position_accessor, i, position, 3)) {
            FR_LOG_ERR("[Cooker] Failed to read glTF vertex position.");
            return false;
        }

        vertex.position[0] = position[0];
        vertex.position[1] = position[1];
        vertex.position[2] = position[2];

        if (normal_accessor) {
            float normal[3] = {0.0f, 1.0f, 0.0f};

            if (!cgltf_accessor_read_float(normal_accessor, i, normal, 3)) {
                FR_LOG_ERR("[Cooker] Failed to read glTF vertex normal.");
                return false;
            }

            vertex.normal[0] = normal[0];
            vertex.normal[1] = normal[1];
            vertex.normal[2] = normal[2];
        } else {
            vertex.normal[0] = 0.0f;
            vertex.normal[1] = 1.0f;
            vertex.normal[2] = 0.0f;
        }

        if (uv_accessor) {
            float uv[2] = {0.0f, 0.0f};

            if (!cgltf_accessor_read_float(uv_accessor, i, uv, 2)) {
                FR_LOG_ERR("[Cooker] Failed to read glTF vertex UV.");
                return false;
            }

            vertex.uv[0] = uv[0];
            vertex.uv[1] = uv[1];
        } else {
            vertex.uv[0] = 0.0f;
            vertex.uv[1] = 0.0f;
        }

        if (tangent_accessor) {
            float tangent[4] = {1.0f, 0.0f, 0.0f, 1.0f};

            if (!cgltf_accessor_read_float(tangent_accessor, i, tangent, 4)) {
                FR_LOG_ERR("[Cooker] Failed to read glTF vertex tangent.");
                return false;
            }

            vertex.tangent[0] = tangent[0];
            vertex.tangent[1] = tangent[1];
            vertex.tangent[2] = tangent[2];
            vertex.tangent[3] = tangent[3];
        } else {
            vertex.tangent[0] = 1.0f;
            vertex.tangent[1] = 0.0f;
            vertex.tangent[2] = 0.0f;
            vertex.tangent[3] = 1.0f;
        }

        out_mesh.vertices.push_back(vertex);

        expand_aabb(submesh.aabb_min, submesh.aabb_max, vertex.position);
        expand_aabb(out_mesh.aabb_min, out_mesh.aabb_max, vertex.position);
    }

    if (primitive.indices) {
        const U32 index_count = static_cast<U32>(primitive.indices->count);

        if (index_count % 3u != 0u) {
            FR_LOG_ERR("[Cooker] Triangle glTF primitive index count is not divisible by 3.");
            return false;
        }

        if (static_cast<USize>(index_count) > static_cast<USize>(-1) - out_mesh.indices.size()) {
            FR_LOG_ERR("[Cooker] Imported mesh index count overflow.");
            return false;
        }

        out_mesh.indices.reserve(out_mesh.indices.size() + static_cast<USize>(index_count));

        for (U32 i = 0; i < index_count; ++i) {
            const U32 index = static_cast<U32>(cgltf_accessor_read_index(primitive.indices, i));
            out_mesh.indices.push_back(index);
        }

        submesh.index_count = index_count;
    } else {
        if (vertex_count % 3u != 0u) {
            FR_LOG_ERR("[Cooker] Non-indexed triangle glTF primitive vertex count is not "
                       "divisible by 3.");
            return false;
        }

        if (static_cast<USize>(vertex_count) > static_cast<USize>(-1) - out_mesh.indices.size()) {
            FR_LOG_ERR("[Cooker] Imported mesh index count overflow.");
            return false;
        }

        out_mesh.indices.reserve(out_mesh.indices.size() + static_cast<USize>(vertex_count));

        for (U32 i = 0; i < vertex_count; ++i) {
            out_mesh.indices.push_back(i);
        }

        submesh.index_count = vertex_count;
    }

    if (!validate_primitive_index_range(out_mesh, submesh)) {
        return false;
    }

    finalize_aabb(submesh.aabb_min, submesh.aabb_max);

    if (!tangent_accessor && uv_accessor && normal_accessor && submesh.index_count > 0) {
        FR_LOG("[Cooker] Generating tangents for primitive: vertices={}, indices={}", vertex_count,
               submesh.index_count);
        generate_missing_tangents(out_mesh, submesh, vertex_count);
    }

    if (primitive.material) {
        AssetId material_id{};

        if (!find_imported_material(imported_materials, primitive.material, material_id)) {
            String material_path;

            if (!cook_gltf_material_asset(data, *primitive.material, base_dir, texture_tasks,
                                          outputs, force, material_path, material_id)) {
                FR_LOG_ERR("[Cooker] Failed to cook glTF material for primitive.");
                return false;
            }

            ImportedMaterialRecord record{};
            record.source = primitive.material;
            record.material_id = material_id;
            record.material_path = std::move(material_path);

            imported_materials.push_back(std::move(record));
        }

        submesh.material_id = material_id;

        if (primitive.material->alpha_mode == cgltf_alpha_mode_mask) {
            submesh.pass_type = 1;
        } else if (primitive.material->alpha_mode == cgltf_alpha_mode_blend) {
            submesh.pass_type = 2;
        }
    }

    out_mesh.submeshes.push_back(submesh);
    return true;
}

static bool import_node(const cgltf_data *data, const cgltf_node *node,
                        const glm::mat4 &parent_transform, StringView base_dir,
                        DynamicArray<TextureBakeTask> &texture_tasks,
                        DynamicArray<ImportedMaterialRecord> &imported_materials,
                        DynamicArray<CookedAssetOutput> *outputs, bool force, RawMesh &out_mesh) {
    if (!node) {
        return true;
    }

    glm::mat4 node_transform = parent_transform * get_node_local_transform(node);

    if (node->mesh) {
        for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i) {
            if (!import_primitive(data, node->mesh->primitives[i], node_transform, base_dir,
                                  texture_tasks, imported_materials, outputs, force, out_mesh)) {
                return false;
            }
        }
    }

    for (cgltf_size i = 0; i < node->children_count; ++i) {
        if (!import_node(data, node->children[i], node_transform, base_dir, texture_tasks,
                         imported_materials, outputs, force, out_mesh)) {
            return false;
        }
    }

    return true;
}

static bool execute_texture_bake_task(const TextureBakeTask &task) noexcept {
    if (!task.needs_bake) {
        return true;
    }

    if (task.type == TextureBakeTaskType::RegularTexture) {
        return cook_texture(task.in_path.view(), task.out_path.view(), task.is_srgb);
    }

    return bake_material_extra_texture(task.in_path.view(), task.secondary_path.view(),
                                       task.out_path.view());
}

/**
 * @brief Processes queued texture baking tasks.
 *
 * @details
 * Texture baking is parallelized because texture import and conversion are independent per output
 * path. Output records are appended sequentially after all worker tasks finish.
 */
static bool bake_queued_textures(DynamicArray<TextureBakeTask> &texture_tasks,
                                 DynamicArray<CookedAssetOutput> *outputs) {
    FR_LOG("[Cooker] Baking queued glTF textures: count={}",
           static_cast<U32>(texture_tasks.size()));

    if (texture_tasks.is_empty()) {
        return true;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    DynamicArray<TextureBakeResult> results(alloc);
    results.grow_default(texture_tasks.size());

    const USize worker_count =
        fr::math::max<USize>(1, fr::math::min<USize>(texture_tasks.size(), static_cast<USize>(8)));

    ThreadPool pool(alloc, worker_count);

    for (USize i = 0; i < texture_tasks.size(); ++i) {
        pool.submit([&texture_tasks, &results, i] {
            const TextureBakeTask &task = texture_tasks[i];
            results[i].ok = execute_texture_bake_task(task);
        });
    }

    pool.wait();

    for (USize i = 0; i < texture_tasks.size(); ++i) {
        const TextureBakeTask &task = texture_tasks[i];

        if (!results[i].ok) {
            FR_LOG_ERR("[Cooker] Failed to bake queued texture task {}: {}", static_cast<U32>(i),
                       task.out_path.view());
            return false;
        }

        append_cooked_asset_output_once(outputs, task.output_id, AssetKind::Texture,
                                        task.out_path.view());
    }

    return true;
}

} // namespace

bool import_gltf(StringView path, RawMesh &out_mesh, DynamicArray<CookedAssetOutput> *outputs,
                 CookOptions options) noexcept {
    g_path_warning_state = {};

    String path_string = String::from_view(path);

    cgltf_options cgltf_options{};
    cgltf_data *data = nullptr;

    cgltf_result parse_result = cgltf_parse_file(&cgltf_options, path_string.c_str(), &data);
    if (parse_result != cgltf_result_success || !data) {
        FR_LOG_ERR("[Cooker] Failed to parse glTF file: {}", path);
        return false;
    }

    cgltf_result load_result = cgltf_load_buffers(&cgltf_options, data, path_string.c_str());
    if (load_result != cgltf_result_success) {
        FR_LOG_ERR("[Cooker] Failed to load glTF buffers: {}", path);
        cgltf_free(data);
        return false;
    }

    FR_LOG("[Cooker] glTF loaded: nodes={}, meshes={}, materials={}, images={}",
           static_cast<U32>(data->nodes_count), static_cast<U32>(data->meshes_count),
           static_cast<U32>(data->materials_count), static_cast<U32>(data->images_count));

    String base_dir = String::from_view(file::get_parent_path(path_string.view()));
    file::normalize_unix(base_dir);

    warn_if_unstable_logical_path(base_dir.view(), "glTF base directory");

    out_mesh.vertices.clear();
    out_mesh.indices.clear();
    out_mesh.submeshes.clear();
    reset_aabb(out_mesh.aabb_min, out_mesh.aabb_max);

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    DynamicArray<CookedAssetOutput> *import_outputs = outputs;

    DynamicArray<TextureBakeTask> texture_tasks(alloc);
    DynamicArray<ImportedMaterialRecord> imported_materials(alloc);

    bool import_ok = true;

    if (data->scene) {
        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) {
            if (!import_node(data, data->scene->nodes[i], glm::mat4(1.0f), base_dir, texture_tasks,
                             imported_materials, import_outputs, options.force, out_mesh)) {
                import_ok = false;
                break;
            }
        }
    } else {
        for (cgltf_size i = 0; i < data->nodes_count; ++i) {
            if (data->nodes[i].parent) {
                continue;
            }

            if (!import_node(data, &data->nodes[i], glm::mat4(1.0f), base_dir, texture_tasks,
                             imported_materials, import_outputs, options.force, out_mesh)) {
                import_ok = false;
                break;
            }
        }
    }

    cgltf_free(data);

    if (!import_ok) {
        return false;
    }

    finalize_aabb(out_mesh.aabb_min, out_mesh.aabb_max);

    FR_LOG("[Cooker] glTF geometry imported: vertices={}, indices={}, submeshes={}, textures={}",
           static_cast<U32>(out_mesh.vertices.size()), static_cast<U32>(out_mesh.indices.size()),
           static_cast<U32>(out_mesh.submeshes.size()), static_cast<U32>(texture_tasks.size()));

    if (!bake_queued_textures(texture_tasks, import_outputs)) {
        FR_LOG_ERR("[Cooker] Failed to bake one or more textures for glTF asset: {}", path);
        return false;
    }

    return true;
}

} // namespace fr::asscooker
