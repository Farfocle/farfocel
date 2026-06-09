
#include "gltf_importer.hpp"

#include "compilers/texture_compiler.hpp"
#include "formats/raw_texture.hpp"

#include "fr/asscooker/asscooker.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"

#include <cgltf.h>
#include <mikktspace.h>
#include <stb_image.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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

    bool is_srgb{false};
};

/**
 * @brief User data passed to MikkTSpace callbacks.
 */
struct MikkUserData {
    RawMesh *mesh{nullptr};
    RawSubMesh *submesh{nullptr};
};

/**
 * @brief Converts std::filesystem::path to fr::String.
 */
static String path_to_string(const std::filesystem::path &path) {
    std::string native = path.string();
    return String::from_chars(native.c_str());
}

/**
 * @brief Prints an image decode failure with filesystem and STB diagnostics.
 */
static void log_texture_decode_failure(StringView path, const char *label) {
    String path_str = String::from_view(path);
    const bool exists = std::filesystem::exists(path_str.data());
    const char *reason = stbi_failure_reason();

    std::cerr << "[Cooker] Failed to load " << (label ? label : "texture") << ".\n"
              << "  path:   " << path_str.data() << '\n'
              << "  exists: " << (exists ? "yes" : "no") << '\n'
              << "  stb:    " << (reason ? reason : "unknown") << '\n';
}

/**
 * @brief Returns true if the same output texture is already queued for baking.
 */
static bool has_pending_texture_task(const DynamicArray<TextureBakeTask> &tasks,
                                     const String &output_path) {
    for (USize i = 0; i < tasks.size(); ++i) {
        if (tasks[i].out_path == output_path) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Queues a regular texture cooking task if the output file does not already exist.
 */
static void queue_regular_texture(DynamicArray<TextureBakeTask> &tasks, const String &input_path,
                                  const String &output_path, bool is_srgb) {
    if (input_path.size() == 0 || output_path.size() == 0) {
        return;
    }

    if (std::filesystem::exists(output_path.data())) {
        return;
    }

    if (has_pending_texture_task(tasks, output_path)) {
        return;
    }

    TextureBakeTask task{};
    task.type = TextureBakeTaskType::RegularTexture;
    task.in_path = input_path;
    task.out_path = output_path;
    task.is_srgb = is_srgb;

    tasks.push_back(task);
}

/**
 * @brief Queues a renderer-specific material extra texture task.
 */
static void queue_material_extra_texture(DynamicArray<TextureBakeTask> &tasks,
                                         const String &metallic_roughness_path,
                                         const String &occlusion_path, const String &output_path) {
    if (output_path.size() == 0) {
        return;
    }

    if (std::filesystem::exists(output_path.data())) {
        return;
    }

    if (has_pending_texture_task(tasks, output_path)) {
        return;
    }

    TextureBakeTask task{};
    task.type = TextureBakeTaskType::MaterialExtraTexture;
    task.in_path = metallic_roughness_path;
    task.secondary_path = occlusion_path;
    task.out_path = output_path;
    task.is_srgb = false;

    tasks.push_back(task);
}

/**
 * @brief Resolves a source texture path from a glTF texture view.
 */
static String get_source_texture_path(const cgltf_texture_view &view,
                                      const std::filesystem::path &base_dir) {
    if (!view.texture || !view.texture->image || !view.texture->image->uri) {
        return String::from_chars("");
    }

    std::filesystem::path full_path = base_dir / std::filesystem::path(view.texture->image->uri);
    return path_to_string(full_path);
}

/**
 * @brief Builds the cooked `.ftex` path for a regular texture.
 */
static String make_regular_ftex_path(const cgltf_texture_view &view,
                                     const std::filesystem::path &base_dir) {
    if (!view.texture || !view.texture->image || !view.texture->image->uri) {
        return String::from_chars("");
    }

    std::filesystem::path output_path = base_dir / std::filesystem::path(view.texture->image->uri);
    output_path.replace_extension(".ftex");

    return path_to_string(output_path);
}

/**
 * @brief Builds the cooked `_extra.ftex` path for a material.
 */
static String make_extra_ftex_path(const cgltf_material &material,
                                   const std::filesystem::path &base_dir) {
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

    std::filesystem::path output_path =
        base_dir / std::filesystem::path(source_view->texture->image->uri);

    std::string stem = output_path.stem().string();
    output_path.replace_filename(stem + "_extra.ftex");

    return path_to_string(output_path);
}

/**
 * @brief Resolves a regular texture and queues it for cooking if necessary.
 */
static String resolve_regular_texture(const cgltf_texture_view &view,
                                      const std::filesystem::path &base_dir,
                                      DynamicArray<TextureBakeTask> &tasks, bool is_srgb) {
    String input_path = get_source_texture_path(view, base_dir);
    String output_path = make_regular_ftex_path(view, base_dir);

    queue_regular_texture(tasks, input_path, output_path, is_srgb);

    return output_path;
}

/**
 * @brief Resolves and queues a renderer-specific material extra texture.
 */
static String resolve_material_extra_texture(const cgltf_material &material,
                                             const std::filesystem::path &base_dir,
                                             DynamicArray<TextureBakeTask> &tasks) {
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

    queue_material_extra_texture(tasks, metallic_roughness_path, occlusion_path, output_path);

    return output_path;
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
        mr_pixels = stbi_load(mr_path.data(), &mr_width, &mr_height, &mr_channels, 4);
        if (!mr_pixels) {
            log_texture_decode_failure(mr_path.view(), "metallic-roughness texture");
            return false;
        }
    }

    if (has_occlusion) {
        ao_pixels = stbi_load(ao_path.data(), &ao_width, &ao_height, &ao_channels, 4);
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
        std::cerr << "[Cooker] Invalid material extra texture dimensions.\n"
                  << "  width:  " << width << '\n'
                  << "  height: " << height << '\n';

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

        std::cerr << "[Cooker] Metallic-roughness and occlusion textures have different sizes.\n"
                  << "  metallic-roughness: " << mr_width << "x" << mr_height << '\n'
                  << "  occlusion:          " << ao_width << "x" << ao_height << '\n';

        stbi_image_free(mr_pixels);
        stbi_image_free(ao_pixels);
        return false;
    }

    RawTexture raw{};
    raw.width = static_cast<U32>(width);
    raw.height = static_cast<U32>(height);
    raw.channels = 4;
    raw.pixel_size = 1;
    raw.format = AssetTextureFormat::RGBA8_UNORM;
    raw.mip_levels = 1 + static_cast<U32>(std::floor(std::log2(std::max(width, height))));

    const USize pixel_count = static_cast<USize>(width) * static_cast<USize>(height);
    raw.pixel_data.grow_default(pixel_count * 4);

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

        raw.pixel_data[dst + 0] = metallic;
        raw.pixel_data[dst + 1] = roughness;
        raw.pixel_data[dst + 2] = occlusion;
        raw.pixel_data[dst + 3] = 255;
    }

    if (mr_pixels) {
        stbi_image_free(mr_pixels);
    }

    if (ao_pixels) {
        stbi_image_free(ao_pixels);
    }

    return compile_texture(raw, output_path);
}

/**
 * @brief Worker function used to process queued texture baking tasks.
 */
static void texture_bake_worker(DynamicArray<TextureBakeTask> *tasks,
                                std::atomic<USize> *next_task_index) {
    while (true) {
        const USize index = next_task_index->fetch_add(1, std::memory_order_relaxed);

        if (index >= tasks->size()) {
            break;
        }

        const TextureBakeTask &task = (*tasks)[index];

        if (task.type == TextureBakeTaskType::RegularTexture) {
            cook_texture(task.in_path.view(), task.out_path.view(), task.is_srgb);
        } else {
            bake_material_extra_texture(task.in_path.view(), task.secondary_path.view(),
                                        task.out_path.view());
        }
    }
}

/**
 * @brief Finds a primitive attribute accessor by type.
 */
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

/**
 * @brief Writes a glm matrix to a raw float array.
 */
static void write_matrix(const glm::mat4 &matrix, F32 *out_matrix) {
    fr::mem::copy_raw_range(glm::value_ptr(matrix), 16, out_matrix);
}

/**
 * @brief Computes a glTF node local transform.
 */
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

/**
 * @brief Resets an AABB to an empty state.
 */
static void reset_aabb(F32 *min_extents, F32 *max_extents) {
    min_extents[0] = 1.0e30f;
    min_extents[1] = 1.0e30f;
    min_extents[2] = 1.0e30f;

    max_extents[0] = -1.0e30f;
    max_extents[1] = -1.0e30f;
    max_extents[2] = -1.0e30f;
}

/**
 * @brief Expands an AABB by one position.
 */
static void expand_aabb(F32 *min_extents, F32 *max_extents, const F32 *position) {
    for (U32 i = 0; i < 3; ++i) {
        min_extents[i] = std::min(min_extents[i], position[i]);
        max_extents[i] = std::max(max_extents[i], position[i]);
    }
}

/**
 * @brief Replaces an empty AABB with a zero-sized box.
 */
static void finalize_aabb(F32 *min_extents, F32 *max_extents) {
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

/**
 * @brief Returns triangle face count for MikkTSpace.
 */
static int mikk_get_num_faces(const SMikkTSpaceContext *context) {
    const MikkUserData *data = static_cast<const MikkUserData *>(context->m_pUserData);
    return static_cast<int>(data->submesh->index_count / 3);
}

/**
 * @brief Returns vertex count per triangle for MikkTSpace.
 */
static int mikk_get_num_vertices_of_face(const SMikkTSpaceContext *, int) {
    return 3;
}

/**
 * @brief Provides vertex position to MikkTSpace.
 */
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

/**
 * @brief Provides vertex normal to MikkTSpace.
 */
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

/**
 * @brief Provides vertex UV to MikkTSpace.
 */
static void mikk_get_tex_coord(const SMikkTSpaceContext *context, float out_tex_coord[], int face,
                               int vertex) {
    MikkUserData *data = static_cast<MikkUserData *>(context->m_pUserData);

    const U32 local_index =
        data->mesh->indices[data->submesh->index_offset + static_cast<U32>(face * 3 + vertex)];

    const RawVertex &raw_vertex = data->mesh->vertices[data->submesh->vertex_offset + local_index];

    out_tex_coord[0] = raw_vertex.uv[0];
    out_tex_coord[1] = raw_vertex.uv[1];
}

/**
 * @brief Accumulates generated tangent data from MikkTSpace.
 */
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

/**
 * @brief Generates missing MikkTSpace tangents for one submesh.
 */
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

/**
 * @brief Imports a single glTF primitive into RawMesh.
 */
static bool import_primitive(const cgltf_primitive &primitive, const glm::mat4 &node_transform,
                             const std::filesystem::path &base_dir,
                             DynamicArray<TextureBakeTask> &texture_tasks, RawMesh &out_mesh) {
    const cgltf_accessor *position_accessor =
        find_attribute(primitive, cgltf_attribute_type_position);

    if (!position_accessor || position_accessor->count == 0) {
        return true;
    }

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

    if (primitive.material) {
        const cgltf_material &material = *primitive.material;

        if (material.alpha_mode == cgltf_alpha_mode_mask) {
            submesh.pass_type = 1;
        } else if (material.alpha_mode == cgltf_alpha_mode_blend) {
            submesh.pass_type = 2;
        }

        if (material.has_pbr_metallic_roughness) {
            submesh.albedo_path = resolve_regular_texture(
                material.pbr_metallic_roughness.base_color_texture, base_dir, texture_tasks, true);
        }

        submesh.normal_path =
            resolve_regular_texture(material.normal_texture, base_dir, texture_tasks, false);

        submesh.extra_path = resolve_material_extra_texture(material, base_dir, texture_tasks);
    }

    const U32 vertex_count = static_cast<U32>(position_accessor->count);
    out_mesh.vertices.reserve(out_mesh.vertices.size() + vertex_count);

    for (U32 i = 0; i < vertex_count; ++i) {
        RawVertex vertex{};

        float position[3] = {0.0f, 0.0f, 0.0f};
        cgltf_accessor_read_float(position_accessor, i, position, 3);

        vertex.position[0] = position[0];
        vertex.position[1] = position[1];
        vertex.position[2] = position[2];

        if (normal_accessor) {
            float normal[3] = {0.0f, 1.0f, 0.0f};
            cgltf_accessor_read_float(normal_accessor, i, normal, 3);

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
            cgltf_accessor_read_float(uv_accessor, i, uv, 2);

            vertex.uv[0] = uv[0];
            vertex.uv[1] = uv[1];
        } else {
            vertex.uv[0] = 0.0f;
            vertex.uv[1] = 0.0f;
        }

        if (tangent_accessor) {
            float tangent[4] = {1.0f, 0.0f, 0.0f, 1.0f};
            cgltf_accessor_read_float(tangent_accessor, i, tangent, 4);

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
        out_mesh.indices.reserve(out_mesh.indices.size() + index_count);

        for (U32 i = 0; i < index_count; ++i) {
            U32 index = static_cast<U32>(cgltf_accessor_read_index(primitive.indices, i));
            out_mesh.indices.push_back(index);
        }

        submesh.index_count = index_count;
    } else {
        out_mesh.indices.reserve(out_mesh.indices.size() + vertex_count);

        for (U32 i = 0; i < vertex_count; ++i) {
            out_mesh.indices.push_back(i);
        }

        submesh.index_count = vertex_count;
    }

    finalize_aabb(submesh.aabb_min, submesh.aabb_max);

    if (!tangent_accessor && uv_accessor && normal_accessor && submesh.index_count > 0) {
        generate_missing_tangents(out_mesh, submesh, vertex_count);
    }

    out_mesh.submeshes.push_back(submesh);
    return true;
}

/**
 * @brief Recursively imports a glTF node hierarchy.
 */
static bool import_node(const cgltf_node *node, const glm::mat4 &parent_transform,
                        const std::filesystem::path &base_dir,
                        DynamicArray<TextureBakeTask> &texture_tasks, RawMesh &out_mesh) {
    if (!node) {
        return true;
    }

    glm::mat4 node_transform = parent_transform * get_node_local_transform(node);

    if (node->mesh) {
        for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i) {
            if (!import_primitive(node->mesh->primitives[i], node_transform, base_dir,
                                  texture_tasks, out_mesh)) {
                return false;
            }
        }
    }

    for (cgltf_size i = 0; i < node->children_count; ++i) {
        if (!import_node(node->children[i], node_transform, base_dir, texture_tasks, out_mesh)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Processes queued texture baking tasks on worker threads.
 */
static void bake_queued_textures(DynamicArray<TextureBakeTask> &texture_tasks) {
    if (texture_tasks.is_empty()) {
        return;
    }

    std::atomic<USize> next_task_index{0};

    U32 worker_count = std::thread::hardware_concurrency();
    if (worker_count == 0) {
        worker_count = 1;
    }

    worker_count = std::min<U32>(worker_count, static_cast<U32>(texture_tasks.size()));

    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for (U32 i = 0; i < worker_count; ++i) {
        workers.emplace_back(texture_bake_worker, &texture_tasks, &next_task_index);
    }

    for (std::thread &worker : workers) {
        worker.join();
    }
}

} // namespace

/**
 * @brief Imports a glTF file into a RawMesh.
 *
 * @param path Source glTF path.
 * @param out_mesh Destination mesh.
 * @return True on success.
 */
bool import_gltf(StringView path, RawMesh &out_mesh) {
    String path_string = String::from_view(path);

    cgltf_options options{};
    cgltf_data *data = nullptr;

    cgltf_result parse_result = cgltf_parse_file(&options, path_string.data(), &data);
    if (parse_result != cgltf_result_success || !data) {
        std::cerr << "[Cooker] Failed to parse glTF file: " << path_string.data() << '\n';
        return false;
    }

    cgltf_result load_result = cgltf_load_buffers(&options, data, path_string.data());
    if (load_result != cgltf_result_success) {
        std::cerr << "[Cooker] Failed to load glTF buffers: " << path_string.data() << '\n';
        cgltf_free(data);
        return false;
    }

    std::filesystem::path gltf_path(path_string.data());
    std::filesystem::path base_dir = gltf_path.parent_path();

    out_mesh.vertices.clear();
    out_mesh.indices.clear();
    out_mesh.submeshes.clear();
    reset_aabb(out_mesh.aabb_min, out_mesh.aabb_max);

    DynamicArray<TextureBakeTask> texture_tasks;

    bool import_ok = true;

    if (data->scene) {
        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) {
            if (!import_node(data->scene->nodes[i], glm::mat4(1.0f), base_dir, texture_tasks,
                             out_mesh)) {
                import_ok = false;
                break;
            }
        }
    } else {
        for (cgltf_size i = 0; i < data->nodes_count; ++i) {
            if (data->nodes[i].parent) {
                continue;
            }

            if (!import_node(&data->nodes[i], glm::mat4(1.0f), base_dir, texture_tasks, out_mesh)) {
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
    bake_queued_textures(texture_tasks);

    return true;
}

} // namespace fr::asscooker
