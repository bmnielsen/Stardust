cmake_minimum_required(VERSION 3.14)
project(Iron)

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_library(Iron SHARED ${SRC_FILES})

target_link_libraries(Iron -L/usr/local/Cellar/llvm/8.0.0/lib BWAPILIB)
target_include_directories(Iron SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_options(Iron PRIVATE -w)
