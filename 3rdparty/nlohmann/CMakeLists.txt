cmake_minimum_required(VERSION 3.24)
project(nlohmann)

set(CMAKE_CXX_STANDARD 23)

add_library(nlohmann INTERFACE)

target_include_directories(nlohmann SYSTEM INTERFACE ${PROJECT_SOURCE_DIR})
