project(farfocel_tests)


set(TEST_SOURCES
    "tests_src/main.cpp"
    "tests_src/allocator.cpp"
    "tests_src/dynamic_array.cpp"
    "tests_src/optional.cpp"
    "tests_src/slice.cpp"
    "tests_src/tuple.cpp"
    "tests_src/string.cpp"
    "tests_src/hash_set.cpp"
)

if(TEST_SOURCES)
    add_executable(farfocel_tests ${TEST_SOURCES})

target_include_directories(farfocel_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(farfocel_tests PRIVATE engine farfocel_compiler_options)

    include(CTest)
    add_test(NAME tests COMMAND farfocel_tests)
else()
    message("No given test source files.")
endif()
