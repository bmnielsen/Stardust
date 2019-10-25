cmake_minimum_required(VERSION 3.14)
project(BWEB)

set(CMAKE_CXX_STANDARD 17)

include_directories(Source)

add_library(BWEB
        Source/Block.cpp
        Source/BWEB.cpp
        Source/BWEBUtil.cpp
        Source/PathFind.cpp
        Source/Station.cpp
        Source/Wall.cpp)

target_link_libraries(BWEB BWAPILIB BWEM)

target_include_directories(BWEB SYSTEM PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_options(BWEB PRIVATE -w)
