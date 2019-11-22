cmake_minimum_required(VERSION 3.14)
project(Newcutus)

set(CMAKE_CXX_STANDARD 17)

set(OPENBW_DIR ${CMAKE_SOURCE_DIR}/3rdparty/openbw/openbw)

IF(MSVC)
    add_definitions(-DWITHOUT_POSIX)
    add_subdirectory(3rdparty/BWAPILIB)
ELSE(MSVC)
    add_subdirectory(3rdparty/openbw/bwapi)
ENDIF(MSVC)

add_subdirectory(3rdparty/BWEM)
add_subdirectory(3rdparty/BWEB)
add_subdirectory(3rdparty/FAP)
add_subdirectory(3rdparty/zstd/build/cmake)
add_subdirectory(src)

IF(NOT MSVC)
    add_subdirectory(test)
ENDIF(NOT MSVC)
