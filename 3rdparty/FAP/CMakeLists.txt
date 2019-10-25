cmake_minimum_required(VERSION 3.14)
project(FAP)

set(CMAKE_CXX_STANDARD 17)

add_library(FAP INTERFACE)

target_include_directories(FAP SYSTEM INTERFACE ${PROJECT_SOURCE_DIR})
