# Farfocel Workspace

cmake_minimum_required(VERSION 3.21)

project(farfocel_workspace LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(farfocel_compiler_options INTERFACE)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/lib")

foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_SOURCE_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_SOURCE_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_SOURCE_DIR}/bin/lib")
endforeach()


set(FARFOCEL_IS_CLANG OFF)
set(FARFOCEL_IS_GNU OFF)


if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(FARFOCEL_IS_CLANG ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(FARFOCEL_IS_GNU ON)
endif()

if(MSVC)
    target_compile_options(farfocel_compiler_options INTERFACE 
        /W4 /WX /permissive- /Zc:preprocessor
    )
else()
    target_compile_options(farfocel_compiler_options INTERFACE 
        -Wall -Wextra -Wpedantic -Werror 
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
    )
endif()

option(ENABLE_ASAN  "Enable AddressSanitizer"           OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_MSAN  "Enable MemorySanitizer"            OFF)
option(ENABLE_TSAN  "Enable ThreadSanitizer"            OFF)

if(ENABLE_MSAN AND NOT FARFOCEL_IS_CLANG)
    message(WARNING "MSan is only supported with Clang; disabling ENABLE_MSAN.")
    set(ENABLE_MSAN OFF)
endif()

if(ENABLE_ASAN AND ENABLE_MSAN)
    message(FATAL_ERROR "ASan and MSan are mutually exclusive.")
endif()

if(ENABLE_TSAN AND (ENABLE_ASAN OR ENABLE_MSAN))
    message(FATAL_ERROR "TSan is mutually exclusive with ASan and MSan.")
endif()

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    target_compile_options(farfocel_compiler_options INTERFACE
        -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls
    )
    target_link_options(farfocel_compiler_options INTERFACE -fsanitize=address)
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    target_compile_options(farfocel_compiler_options INTERFACE
        -fsanitize=undefined -fno-omit-frame-pointer
    )
    target_link_options(farfocel_compiler_options INTERFACE -fsanitize=undefined)
endif()

if(ENABLE_MSAN)
    message(STATUS "MemorySanitizer enabled")
    target_compile_options(farfocel_compiler_options INTERFACE
        -fsanitize=memory -fsanitize-memory-track-origins=2 
        -fno-omit-frame-pointer -fno-optimize-sibling-calls
    )
    target_link_options(farfocel_compiler_options INTERFACE -fsanitize=memory)
endif()

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    target_compile_options(farfocel_compiler_options INTERFACE
        -fsanitize=thread -fno-omit-frame-pointer
    )
    target_link_options(farfocel_compiler_options INTERFACE -fsanitize=thread)
endif()

add_subdirectory(3rdparty)
add_subdirectory(engine)
add_subdirectory(sandbox)
add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(docs)
