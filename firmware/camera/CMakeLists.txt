# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_definitions(DETOOLS_CONFIG_FILE_IO=0)
add_compile_definitions(DETOOLS_CONFIG_COMPRESSION_NONE=0)
add_compile_definitions(DETOOLS_CONFIG_COMPRESSION_LZMA=0)
add_compile_definitions(DETOOLS_CONFIG_COMPRESSION_CRLE=1)
add_compile_definitions(DETOOLS_CONFIG_COMPRESSION_HEATSHRINK=1)

project(camera)
