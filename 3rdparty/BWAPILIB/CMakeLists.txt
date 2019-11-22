cmake_minimum_required(VERSION 3.14)
project(BWAPILIB)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/Source *.cpp)

add_library(BWAPILIB ${SRC_FILES})

target_include_directories(BWAPILIB SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_options(BWAPILIB PRIVATE -w)
