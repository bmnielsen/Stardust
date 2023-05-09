cmake_minimum_required(VERSION 3.24)
project(Stardust)

set(CMAKE_CXX_STANDARD 17)

IF(MSVC)
    include(CheckIPOSupported)
    check_ipo_supported()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

    option(TOURNAMENT_BUILD "Tournament build" OFF)

    IF(NOT TOURNAMENT_BUILD)
        set(ZSTD_BUILD_CONTRIB false)
        set(ZSTD_BUILD_PROGRAMS false)
        set(ZSTD_BUILD_SHARED false)
        set(ZSTD_BUILD_STATIC true)
        set(ZSTD_BUILD_TESTS false)
        set(ZSTD_LEGACY_SUPPORT false)
        set(ZSTD_LZMA_SUPPORT false)
        set(ZSTD_MULTITHREAD_SUPPORT false)
        set(ZSTD_USE_STATIC_RUNTIME true)
        set(ZSTD_ZLIB_SUPPORT false)
    ENDIF(NOT TOURNAMENT_BUILD)

    add_definitions(-DWITHOUT_POSIX)
    add_subdirectory(3rdparty/BWAPILIB)
ELSE(MSVC)
    set(OPENBW_DIR ${CMAKE_SOURCE_DIR}/3rdparty/openbw/openbw)
    add_subdirectory(3rdparty/openbw/bwapi)
ENDIF(MSVC)

add_subdirectory(3rdparty/BWEM)
add_subdirectory(3rdparty/FAP)
add_subdirectory(3rdparty/nlohmann)

IF(NOT MSVC OR NOT TOURNAMENT_BUILD)
    add_subdirectory(3rdparty/zstd/build/cmake)
ENDIF(NOT MSVC OR NOT TOURNAMENT_BUILD)

add_subdirectory(src)

IF(MSVC)
    IF(NOT TOURNAMENT_BUILD)
        add_subdirectory(windbg)
    ENDIF(NOT TOURNAMENT_BUILD)
ELSE(MSVC)
    add_subdirectory(test)
ENDIF(MSVC)
