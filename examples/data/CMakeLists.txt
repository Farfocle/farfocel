add_executable(fr_example_data_imgui_inspector imgui_inspector.cpp)
target_link_libraries(fr_example_data_imgui_inspector
    PRIVATE
        engine
        imgui
        glad
        SDL3::SDL3-static
        farfocel_compiler_options
)
target_include_directories(fr_example_data_imgui_inspector
    PRIVATE "${CMAKE_SOURCE_DIR}/3rdparty/glad/include"
)

set(BENCH_SOURCES
    "async_query.cpp"
    "console_game_loop.cpp"
    "registry.cpp"
    "registry_commands.cpp"
    "scripts.cpp"
    "world.cpp"
    "hierarchy.cpp"
    "showcase.cpp"
    "resources.cpp"
    "serialization.cpp"
    "time_travel.cpp"
)

foreach(source ${BENCH_SOURCES})
  get_filename_component(name ${source} NAME_WE)
  set(exe_name "fr_example_data_${name}")

  add_executable(${exe_name} ${source})
  target_link_libraries(${exe_name}
        PRIVATE
            engine
            farfocel_compiler_options
    )
endforeach()
