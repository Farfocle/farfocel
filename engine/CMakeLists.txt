project(farfocel_engine)


set(ENGINE_HEADERS
#### core
    "include/fr/core/allocator.hpp"
    "include/fr/core/allocator_typedefs.hpp"
    "include/fr/core/allocation_stack.hpp"
    "include/fr/core/arena_allocator.hpp"
    "include/fr/core/block_allocator.hpp"
    "include/fr/core/dynamic_array.hpp"
    "include/fr/core/globals.hpp"
    "include/fr/core/impl.hpp"
    "include/fr/core/macros.hpp"
    "include/fr/core/malloc_allocator.hpp"
    "include/fr/core/mem.hpp"
    "include/fr/core/new_delete_allocator.hpp"
    "include/fr/core/slice.hpp"
    "include/fr/core/string_view.hpp"
    "include/fr/core/string_base.hpp"
    "include/fr/core/time.hpp"
    "include/fr/core/tuple.hpp"
    "include/fr/core/typedefs.hpp"
    "include/fr/core/unique_ptr.hpp"
)

set(ENGINE_SOURCES
#### core
    "src/core/globals.cpp"
)


add_library(engine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

set_target_properties(engine PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(engine PRIVATE farfocel_compiler_options)

target_include_directories(engine
    PUBLIC "include"
    PRIVATE "src"

)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(engine PRIVATE FR_STL_EXPORT)
else()
    target_compile_definitions(engine PUBLIC FR_STL_STATIC)
endif()
