set(BENCH_SOURCES
    "console_game_loop.cpp"
    "registry.cpp"
    "registry_commands.cpp"
    "scripts.cpp"
    "world.cpp"
    "hierarchy.cpp"
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
