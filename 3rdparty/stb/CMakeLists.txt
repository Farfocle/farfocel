add_library(stb STATIC 
    src/stb_image.cpp
)
target_include_directories(stb PUBLIC include)

target_compile_options(stb PRIVATE -w)
