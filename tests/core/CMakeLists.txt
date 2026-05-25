# Core test suite

set(TEST_CORE_SOURCES
    "main.cpp"
    "alloc.cpp"
    "array.cpp"
    "bitset.cpp"
    "dynamic_array.cpp"
    "optional.cpp"
    "pair.cpp"
    "slice.cpp"
    "stack.cpp"
    "tuple.cpp"
    "string.cpp"
    "hash_set.cpp"
    "hash_map.cpp"
    "inline_any.cpp"
    "inline_function.cpp"
    "serialization.cpp"
    "format.cpp"
)


add_executable(farfocel_test_core ${TEST_CORE_SOURCES})

target_link_libraries(farfocel_test_core
    PRIVATE
        engine
        farfocel_compiler_options
        doctest
)

include(CTest)
add_test(NAME test_core COMMAND farfocel_test_core)
