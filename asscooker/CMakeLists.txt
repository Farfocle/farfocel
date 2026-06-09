project(farfocel_asscooker)

set(COOKER_HEADERS
    "include/fr/asscooker/asscooker.hpp"
)

set(COOKER_SOURCES
    "src/asscooker.cpp"
    "src/importers/gltf_importer.cpp"
    "src/importers/stb_importer.cpp"
    "src/compilers/mesh_compiler.cpp"
    "src/compilers/texture_compiler.cpp"
)

add_library(asscooker STATIC ${COOKER_SOURCES} ${COOKER_HEADERS})

target_include_directories(asscooker
    PUBLIC "include"
    PRIVATE "src"
)

target_link_libraries(asscooker PUBLIC engine)
target_link_libraries(asscooker PRIVATE cgltf stb bc7enc mikktspace)
