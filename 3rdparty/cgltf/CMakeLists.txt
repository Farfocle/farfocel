cmake_minimum_required(VERSION 3.15)
project(cgltf_project)

add_library(cgltf STATIC cgltf.c)

target_include_directories(cgltf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(cgltf PRIVATE -w)
