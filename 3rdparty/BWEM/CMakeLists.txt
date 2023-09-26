cmake_minimum_required(VERSION 3.24)
project(BWEM)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_library(BWEM
        src/area.cpp
        src/base.cpp
        src/bwapiExt.cpp
        src/bwem.cpp
        src/cp.cpp
        src/graph.cpp
        src/gridMap.cpp
        src/map.cpp
        src/mapDrawer.cpp
        src/mapImpl.cpp
        src/mapPrinter.cpp
        src/neutral.cpp
        src/tiles.cpp
        src/utils.cpp
        src/BaseFinder.cpp)

target_link_libraries(BWEM PUBLIC BWAPILIB)

target_include_directories(BWEM SYSTEM PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_options(BWEM PRIVATE -w)
