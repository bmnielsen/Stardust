cmake_minimum_required(VERSION 3.14)
project(Locutus)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty)

set(LOCUTUS_INCLUDES
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/Builder
        ${PROJECT_SOURCE_DIR}/General
        ${PROJECT_SOURCE_DIR}/Instrumentation
        ${PROJECT_SOURCE_DIR}/Map
        ${PROJECT_SOURCE_DIR}/Map/PathFinding
        ${PROJECT_SOURCE_DIR}/Players
        ${PROJECT_SOURCE_DIR}/Producer
        ${PROJECT_SOURCE_DIR}/Strategist
        ${PROJECT_SOURCE_DIR}/Units
        ${PROJECT_SOURCE_DIR}/Util
        ${PROJECT_SOURCE_DIR}/Workers
        )
include_directories(${LOCUTUS_INCLUDES})

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR} *.cpp)

add_library(Locutus SHARED ${SRC_FILES})

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(Locutus PUBLIC DEBUG=1)
endif ()

IF (MSVC)
    target_link_libraries(Locutus BWAPILIB BWEM FAP libzstd_static)
ELSE (MSVC)
    target_link_libraries(Locutus -L/usr/local/Cellar/llvm/8.0.0/lib BWAPILIB BWEM FAP libzstd_static c++fs)
ENDIF (MSVC)

target_include_directories(Locutus PUBLIC ${LOCUTUS_INCLUDES})

IF (NOT MSVC)
    target_compile_options(Locutus PRIVATE -Wall -Werror)
ENDIF (NOT MSVC)
