
cmake_minimum_required(VERSION 3.21)
project(mikktspace LANGUAGES C)
add_library(mikktspace STATIC mikktspace.c)

target_include_directories(mikktspace PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
