project(fr_example_core_imgui_archive)

add_executable(fr_example_core_imgui_archive main.cpp)

target_link_libraries(fr_example_core_imgui_archive
    PRIVATE
        engine
        imgui
        glad
        SDL3::SDL3-static
        farfocel_compiler_options
)

target_include_directories(fr_example_core_imgui_archive
    PRIVATE "${CMAKE_SOURCE_DIR}/3rdparty/glad/include"
)
