cmake_minimum_required(VERSION 3.24)
project(Iron)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_library(Iron SHARED ${SRC_FILES})

target_link_libraries(Iron BWAPILIB)
target_include_directories(Iron SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_options(Iron PRIVATE -w)
