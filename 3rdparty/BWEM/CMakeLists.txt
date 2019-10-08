cmake_minimum_required(VERSION 3.14)
project(BWEM)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/include)
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
