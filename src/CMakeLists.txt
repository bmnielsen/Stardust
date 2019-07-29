cmake_minimum_required(VERSION 3.14)
project(Locutus)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/include)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty/BWEB)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty/BWEM)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty/CQL)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty/FAP)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty)

set(LOCUTUS_INCLUDES
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/Builder
        ${PROJECT_SOURCE_DIR}/General
        ${PROJECT_SOURCE_DIR}/Instrumentation
        ${PROJECT_SOURCE_DIR}/Map
        ${PROJECT_SOURCE_DIR}/Players
        ${PROJECT_SOURCE_DIR}/Strategist
        ${PROJECT_SOURCE_DIR}/Units
        ${PROJECT_SOURCE_DIR}/Util
        )
include_directories(${LOCUTUS_INCLUDES})

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR} *.cpp)

add_library(Locutus SHARED ${SRC_FILES})

target_link_libraries(Locutus -L/usr/local/Cellar/llvm/8.0.0/lib BWAPILIB BWEM BWEB libzstd_static c++fs)
target_include_directories(Locutus PUBLIC ${LOCUTUS_INCLUDES})