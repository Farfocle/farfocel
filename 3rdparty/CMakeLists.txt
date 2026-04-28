# 3rdparty libraries

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/doctest")

add_library(nanobench INTERFACE)
target_include_directories(nanobench INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nanobench")
