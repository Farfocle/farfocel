# Benchmarks

set(BENCHMARK_SOURCES
    "hash_set.cpp"
)

foreach(source ${BENCHMARK_SOURCES})
    get_filename_component(name ${source} NAME_WE)
    set(exe_name "farfocel_${name}")
    
    add_executable(${exe_name} ${source})
    target_link_libraries(${exe_name} 
        PRIVATE 
            engine 
            farfocel_compiler_options 
            nanobench
    )
endforeach()
