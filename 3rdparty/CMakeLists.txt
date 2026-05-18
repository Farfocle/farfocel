# 3rdparty libraries

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/doctest")

add_library(nanobench INTERFACE)
target_include_directories(nanobench INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nanobench")

# Markes as SYSTEM - supresses strict C++ compiler warnings
add_library(yyjson STATIC yyjson/yyjson.c)
target_include_directories(yyjson SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/yyjson")

add_subdirectory(glad)
add_subdirectory(glm)

set(SDL_SHARED OFF CACHE BOOL "Do not build SDL as a shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL as a static library" FORCE)
add_subdirectory(SDL)
add_subdirectory(cgltf)

