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
    "include/fr/core/stack.hpp"
    "include/fr/core/string.hpp"
    "include/fr/core/string_base.hpp"
    "include/fr/core/string_view.hpp"
    "include/fr/core/time.hpp"
    "include/fr/core/tuple.hpp"
    "include/fr/core/typedefs.hpp"
    "include/fr/core/typetraits.hpp"
    "include/fr/core/unique_ptr.hpp"

### data
    "include/fr/data/cmd.hpp"
    "include/fr/data/part.hpp"
    "include/fr/data/part_pool.hpp"
    "include/fr/data/query.hpp"
    "include/fr/data/registry.hpp"
    "include/fr/data/scope.hpp"
    "include/fr/data/script.hpp"
    "include/fr/data/signature_pool.hpp"
    "include/fr/data/stage.hpp"
    "include/fr/data/thing.hpp"
    "include/fr/data/thing_pool.hpp"
    "include/fr/data/world.hpp"
    "include/fr/data/relations.hpp"
)

set(ENGINE_SOURCES
#### core
    "src/core/ctx.cpp"
#### data
    "src/data/cmd.cpp"
)


add_library(engine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

set_target_properties(engine PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(engine PRIVATE farfocel_compiler_options)
target_link_libraries(engine PUBLIC yyjson)

target_include_directories(engine
    PUBLIC "include"
    PRIVATE "src"

)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(engine PRIVATE FR_STL_EXPORT)
else()
  target_compile_definitions(engine PUBLIC FR_STL_STATIC)
endif()
