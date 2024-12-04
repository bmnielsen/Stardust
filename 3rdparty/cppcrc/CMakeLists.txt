cmake_minimum_required(VERSION 3.24)
project(cppcrc)

set(CMAKE_CXX_STANDARD 20)

add_library(cppcrc INTERFACE)

target_include_directories(cppcrc SYSTEM INTERFACE ${PROJECT_SOURCE_DIR})
