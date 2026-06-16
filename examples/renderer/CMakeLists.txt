set(RENDERER_EXAMPLE_SOURCES
    "testbed.cpp"
)

foreach(source ${RENDERER_EXAMPLE_SOURCES})
    get_filename_component(name ${source} NAME_WE)
    set(exe_name "farfocel_example_renderer_${name}")

    add_executable(${exe_name} ${source})

    target_link_libraries(${exe_name}
        PRIVATE
            farfocel_compiler_options
            engine
            asscooker
            imgui 
            imguizmo
            SDL3::SDL3-static 
            glad
    )
endforeach()
