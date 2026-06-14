project(farfocel_asscooker)

find_package(Threads REQUIRED)

set(COOKER_HEADERS
    "include/fr/asscooker/asscooker.hpp"

    "src/compilers/manifest_compiler.hpp"
    "src/compilers/material_compiler.hpp"
    "src/compilers/mesh_compiler.hpp"
    "src/compilers/pack_compiler.hpp"
    "src/compilers/shader_compiler.hpp"
    "src/compilers/texture_compiler.hpp"

    "src/formats/raw_material.hpp"
    "src/formats/raw_mesh.hpp"
    "src/formats/raw_shader.hpp"
    "src/formats/raw_texture.hpp"

    "src/importers/gltf_importer.hpp"
    "src/importers/stb_importer.hpp"
)

set(COOKER_SOURCES
    "src/compilers/manifest_compiler.cpp"
    "src/compilers/material_compiler.cpp"
    "src/compilers/mesh_compiler.cpp"
    "src/compilers/pack_compiler.cpp"
    "src/compilers/shader_compiler.cpp"
    "src/compilers/texture_compiler.cpp"

    "src/importers/gltf_importer.cpp"
    "src/importers/stb_importer.cpp"

    "src/asscooker.cpp"
)

add_library(asscooker STATIC ${COOKER_SOURCES} ${COOKER_HEADERS})

target_include_directories(asscooker
    PUBLIC
        "include"

    PRIVATE
        "src"
)

target_link_libraries(asscooker
    PUBLIC
        engine

    PRIVATE
        cgltf
        stb
        bc7enc
        mikktspace
        Threads::Threads
)
