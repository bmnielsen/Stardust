cmake_minimum_required(VERSION 3.24)
project(zstdstream)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty)

add_library(zstdstream zstdstream.cpp)

target_link_libraries(zstdstream PUBLIC libzstd_static)

target_include_directories(zstdstream SYSTEM PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_options(zstdstream PRIVATE -w)
