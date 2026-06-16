set(RENDERER_EXAMPLE_SOURCES
    "inspector.cpp"
    "physics.cpp"
)

foreach(source ${RENDERER_EXAMPLE_SOURCES})
    get_filename_component(name ${source} NAME_WE)
    set(exe_name "farfocel_demo_${name}")

    add_executable(${exe_name} ${source})

    target_link_libraries(${exe_name}
        PRIVATE
            farfocel_compiler_options
            engine
            asscooker
            imgui 
            SDL3::SDL3-static 
            glad
    )
endforeach()

