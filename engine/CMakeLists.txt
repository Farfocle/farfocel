project(farfocel_engine)


set(ENGINE_HEADERS
#### core
    "include/fr/core/algo.hpp"
    "include/fr/core/alloc.hpp"
    "include/fr/core/alloc_tracer.hpp"
    "include/fr/core/alloc_typedefs.hpp"
    "include/fr/core/arena_alloc.hpp"
    "include/fr/core/array.hpp"
    "include/fr/core/bitset.hpp"
    "include/fr/core/block_alloc.hpp"
    "include/fr/core/ctx.hpp"
    "include/fr/core/dynamic_array.hpp"
    "include/fr/core/file.hpp"
    "include/fr/core/format.hpp"
    "include/fr/core/hash.hpp"
    "include/fr/core/hash_map.hpp"
    "include/fr/core/hash_set.hpp"
    "include/fr/core/heap_alloc.hpp"
    "include/fr/core/inline_any.hpp"
    "include/fr/core/inline_function.hpp"
    "include/fr/core/json.hpp"
    "include/fr/core/macros.hpp"
    "include/fr/core/malloc_alloc.hpp"
    "include/fr/core/math.hpp"
    "include/fr/core/mem.hpp"
    "include/fr/core/meta.hpp"
    "include/fr/core/new_delete_alloc.hpp"
    "include/fr/core/nil.hpp"
    "include/fr/core/optional.hpp"
    "include/fr/core/pair.hpp"
    "include/fr/core/queue.hpp"
    "include/fr/core/shape.hpp"
    "include/fr/core/slice.hpp"
    "include/fr/core/slot_map.hpp"
    "include/fr/core/stack.hpp"
    "include/fr/core/string.hpp"
    "include/fr/core/string_base.hpp"
    "include/fr/core/string_view.hpp"
    "include/fr/core/strong_handle.hpp"
    "include/fr/core/thread_pool.hpp"
    "include/fr/core/time.hpp"
    "include/fr/core/timestamp.hpp"
    "include/fr/core/tuple.hpp"
    "include/fr/core/typedefs.hpp"
    "include/fr/core/typetraits.hpp"
    "include/fr/core/unique_ptr.hpp"

### data
    "include/fr/data/asset_format.hpp"
    "include/fr/data/asset_manager.hpp"
    "include/fr/data/cmd.hpp"
    "include/fr/data/part.hpp"
    "include/fr/data/parts.hpp"
    "include/fr/data/registry.hpp"
    "include/fr/data/resource.hpp"
    "include/fr/data/thing.hpp"
    "include/fr/data/world.hpp"

### logger
    "include/fr/logger/sinks/pretty_sink.hpp"
    "include/fr/logger/sinks/standard_sink.hpp"
    "include/fr/logger/log.hpp"
    "include/fr/logger/logger.hpp"
    "include/fr/logger/sink.hpp"

#### physics
    "include/fr/physics/collision_manifold_pool.hpp"
    "include/fr/physics/parts.hpp"
    "include/fr/physics/resources.hpp"
    "include/fr/physics/spatial_hash_grid.hpp"
    "include/fr/physics/systems.hpp"

#### platform
    "include/fr/platform/input.hpp"
    "include/fr/platform/keycode.hpp"
    "include/fr/platform/window.hpp"

#### devtools
    "include/fr/devtools/imgui_archive.hpp"
    "include/fr/devtools/inspector.hpp"

#### renderer
    "include/fr/renderer/frustum.hpp"
    "include/fr/renderer/mesh.hpp"
    "include/fr/renderer/render_bindings.hpp"
    "include/fr/renderer/render_device.hpp"
    "include/fr/renderer/render_light_data.hpp"
    "include/fr/renderer/render_queue.hpp"
    "include/fr/renderer/render_sort_key.hpp"
    "include/fr/renderer/renderer.hpp"
    "include/fr/renderer/renderer_constants.hpp"
    "include/fr/renderer/renderer_desc.hpp"
    "include/fr/renderer/renderer_frame_data.hpp"
    "include/fr/renderer/renderer_resources.hpp"

### scene
    "include/fr/scene/camera_system.hpp"
    "include/fr/scene/render_parts.hpp"
    "include/fr/scene/render_system.hpp"
)

set(ENGINE_SOURCES
#### core
    "src/core/ctx.cpp"
    "src/core/hash.cpp"

#### data
    "src/data/cmd.cpp"

#### logger
    "src/logger/sinks/pretty_sink.cpp"
    "src/logger/sinks/standard_sink.cpp"
    "src/logger/logger.cpp"

#### physics
    "src/physics/systems.cpp"

#### platform
    "src/platform/sdl3/sdl_window.cpp"

#### renderer
    "src/renderer/opengl/opengl_render_device.cpp"
    "src/renderer/renderer.cpp"
    "src/renderer/renderer_passes.cpp"
)


add_library(engine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

set_target_properties(engine PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(engine PRIVATE farfocel_compiler_options)

# should this be public?
target_link_libraries(engine PUBLIC yyjson)

target_link_libraries(engine PRIVATE glad)
target_link_libraries(engine PUBLIC glm::glm)
target_link_libraries(engine PUBLIC SDL3::SDL3-static)
target_link_libraries(engine PRIVATE cgltf)
target_link_libraries(engine PRIVATE stb)
target_link_libraries(engine PUBLIC wyhash)
target_link_libraries(engine PUBLIC imgui)

target_include_directories(engine
    PUBLIC "include"
    PRIVATE "src"
    # this is quite ugly, but i couldn't get aliases to work properly
    "${CMAKE_SOURCE_DIR}/3rdparty/glad/include"
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(engine PRIVATE FR_STL_EXPORT)
else()
  target_compile_definitions(engine PUBLIC FR_STL_STATIC)
endif()
