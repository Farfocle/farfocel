# Core test suite

set(CORE_TEST_SOURCES
    "main.cpp"
    "allocator.cpp"
    "dynamic_array.cpp"
    "optional.cpp"
    "pair.cpp"
    "slice.cpp"
    "tuple.cpp"
    "string.cpp"
    "hash_set.cpp"
    "hash_map.cpp"
)

add_executable(farfocel_core_tests ${CORE_TEST_SOURCES})

target_link_libraries(farfocel_core_tests 
    PRIVATE 
        engine 
        farfocel_compiler_options 
        doctest
)

include(CTest)
add_test(NAME core_tests COMMAND farfocel_core_tests)
