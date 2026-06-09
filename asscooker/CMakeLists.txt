project(farfocel_asscooker)

set(COOKER_HEADERS
    "include/fr/asscooker/asscooker.hpp"
    "src/compilers/mesh_compiler.hpp"
    "src/compilers/texture_compiler.hpp"
    "src/formats/raw_mesh.hpp"
    "src/formats/raw_texture.hpp"
    "src/importers/gltf_importer.hpp"
    "src/importers/stb_importer.hpp"
)

set(COOKER_SOURCES
    "src/compilers/mesh_compiler.cpp"
    "src/compilers/texture_compiler.cpp"
    "src/importers/gltf_importer.cpp"
    "src/importers/stb_importer.cpp"
    "src/asscooker.cpp"
)

add_library(asscooker STATIC ${COOKER_SOURCES} ${COOKER_HEADERS})

target_include_directories(asscooker
    PUBLIC "include"
    PRIVATE "src"
)

target_link_libraries(asscooker PUBLIC engine)
target_link_libraries(asscooker PRIVATE cgltf stb bc7enc mikktspace)
