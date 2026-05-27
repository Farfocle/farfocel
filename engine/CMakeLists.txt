project(farfocel_engine)


set(ENGINE_HEADERS
#### core
    "include/fr/core/alloc_tracer.hpp"
    "include/fr/core/alloc.hpp"
    "include/fr/core/alloc_typedefs.hpp"
    "include/fr/core/arena_alloc.hpp"
    "include/fr/core/array.hpp"
    "include/fr/core/block_alloc.hpp"
    "include/fr/core/dynamic_array.hpp"
    "include/fr/core/hash.hpp"
    "include/fr/core/hash_map.hpp"
    "include/fr/core/hash_set.hpp"
    "include/fr/core/heap_alloc.hpp"
    "include/fr/core/macros.hpp"
    "include/fr/core/malloc_alloc.hpp"
    "include/fr/core/math.hpp"
    "include/fr/core/mem.hpp"
    "include/fr/core/new_delete_alloc.hpp"
    "include/fr/core/nil.hpp"
    "include/fr/core/optional.hpp"
    "include/fr/core/pair.hpp"
    "include/fr/core/slice.hpp"
    "include/fr/core/string.hpp"
    "include/fr/core/string_base.hpp"
    "include/fr/core/string_view.hpp"
    "include/fr/core/time.hpp"
    "include/fr/core/tuple.hpp"
    "include/fr/core/typedefs.hpp"
    "include/fr/core/unique_ptr.hpp"
    "include/fr/core/slot_map.hpp"
#### platform
    "include/fr/platform/window.hpp"
    "include/fr/platform/keycode.hpp"
    "include/fr/platform/input.hpp"

#### renderer
    "include/fr/renderer/render_device.hpp"
    "include/fr/renderer/mesh_loader.hpp"
    "include/fr/renderer/mesh.hpp"
    "include/fr/renderer/camera.hpp"
    "include/fr/renderer/render_queue.hpp"
    "include/fr/renderer/renderer.hpp"
)

set(ENGINE_SOURCES
#### core
    "src/core/ctx.cpp"
#### platform
    "src/platform/sdl3/sdl_window.cpp"
    
#### renderer
    "src/renderer/opengl/opengl_render_device.cpp"
    "src/renderer/mesh.cpp"
    "src/renderer/mesh_loader.cpp"
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

target_include_directories(engine
    PUBLIC "include"
    PRIVATE "src"
    # this is quite ugly, but i couldn't get aliases to work properly
    PRIVATE "${CMAKE_SOURCE_DIR}/3rdparty/glad/include"
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(engine PRIVATE FR_STL_EXPORT)
else()
    target_compile_definitions(engine PUBLIC FR_STL_STATIC)
endif()
