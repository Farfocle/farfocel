set(BENCH_SOURCES
    "array.cpp"
    "bitset.cpp"
    "dynamic_array.cpp"
    "format.cpp"
    "hash_map.cpp"
    "hash_set.cpp"
    "optional.cpp"
    "pair_tuple.cpp"
    "radix_sort.cpp"
    "serialization.cpp"
    "slice.cpp"
    "unique_ptr.cpp"
    "file_helpers.cpp"
)

foreach(source ${BENCH_SOURCES})
    get_filename_component(name ${source} NAME_WE)
    set(exe_name "farfocel_example_core_${name}")

    add_executable(${exe_name} ${source})
    target_link_libraries(${exe_name}
        PRIVATE
            engine
            farfocel_compiler_options
    )
endforeach()
