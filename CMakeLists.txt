cmake_minimum_required(VERSION 3.14)
project(Newcutus)

set(CMAKE_CXX_STANDARD 17)

set(OPENBW_DIR ${CMAKE_SOURCE_DIR}/3rdparty/openbw/openbw)

add_subdirectory(3rdparty/openbw/bwapi)
add_subdirectory(3rdparty/BWEM)
add_subdirectory(3rdparty/BWEB)
add_subdirectory(3rdparty/FAP)
add_subdirectory(3rdparty/zstd/build/cmake)
add_subdirectory(src)
add_subdirectory(test)
