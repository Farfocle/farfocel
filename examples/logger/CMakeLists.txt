set(BENCH_SOURCES
    "logger.cpp"
)

foreach(source ${BENCH_SOURCES})
    get_filename_component(name ${source} NAME_WE)
    set(exe_name "farfocel_example_logger_${name}")

    add_executable(${exe_name} ${source})
    target_link_libraries(${exe_name}
        PRIVATE
            engine
            farfocel_compiler_options
    )
endforeach()
