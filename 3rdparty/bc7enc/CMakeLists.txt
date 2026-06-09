
cmake_minimum_required(VERSION 3.21)
project(bc7enc LANGUAGES C)
add_library(bc7enc STATIC bc7enc.c)

target_include_directories(bc7enc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
