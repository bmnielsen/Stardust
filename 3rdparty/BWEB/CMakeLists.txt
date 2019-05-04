cmake_minimum_required(VERSION 3.14)
project(BWEB)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/include)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty/BWEM)
include_directories(Source)

add_library(BWEB
        Source/Block.cpp
        Source/BWEB.cpp
        Source/BWEBUtil.cpp
        Source/PathFind.cpp
        Source/Station.cpp
        Source/Wall.cpp)

target_link_libraries(BWEM)