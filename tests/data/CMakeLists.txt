# Data test suite

set(TEST_DATA_SOURCES
    "main.cpp"
    "world.cpp"
    "world_parts.cpp"
    "world_relations.cpp"
    "world_queries.cpp"
    "world_resources.cpp"
    "world_cmds.cpp"
)


add_executable(farfocel_test_data ${TEST_DATA_SOURCES})

target_link_libraries(farfocel_test_data
    PRIVATE
        engine
        farfocel_compiler_options
        doctest
)

include(CTest)
add_test(NAME test_data COMMAND farfocel_test_data)
