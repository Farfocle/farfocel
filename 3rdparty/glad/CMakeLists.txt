cmake_minimum_required(VERSION 3.15)
project(glad_project C)

add_library(glad STATIC src/gl.c)

target_include_directories(glad PUBLIC include)
