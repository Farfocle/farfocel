project(sandbox)

set(SANDBOX_SOURCES
    "src/dynamic_array.cpp" 
)

if(SANDBOX_SOURCES)
    add_executable(sandbox ${SANDBOX_SOURCES})

    target_link_libraries(sandbox PRIVATE farfocel_compiler_options)
    target_link_libraries(sandbox PRIVATE engine)
    target_include_directories(sandbox PRIVATE "src")
else()
    message("No source files found for Sandbox. Skipping.")
endif()