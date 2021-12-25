cmake_minimum_required(VERSION 3.14)
project(Locutus)

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/Locutus/Source/*.cpp)
file(GLOB_RECURSE BOSS_SRC_FILES ${PROJECT_SOURCE_DIR}/BOSS/source/*.cpp)
file(GLOB_RECURSE BWTA_SRC_FILES ${PROJECT_SOURCE_DIR}/BWTA/src/*.cpp)
file(GLOB_RECURSE BWEM_SRC_FILES ${PROJECT_SOURCE_DIR}/BWEM/src/*.cpp)
file(GLOB_RECURSE BWEB_SRC_FILES ${PROJECT_SOURCE_DIR}/BWEB/src/*.cpp)

set(LOCUTUS_INCLUDES
        ${PROJECT_SOURCE_DIR}/BWEB/src
        ${PROJECT_SOURCE_DIR}/BWEM/include
        ${PROJECT_SOURCE_DIR}/BWTA/interface
        )
include_directories(${LOCUTUS_INCLUDES})

add_library(Locutus SHARED ${SRC_FILES} ${BOSS_SRC_FILES} ${BWTA_SRC_FILES} ${BWEM_SRC_FILES} ${BWEB_SRC_FILES})

target_link_libraries(Locutus -L/usr/local/Cellar/llvm/13.0.0_2/lib BWAPILIB)
target_include_directories(Locutus SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/Locutus/Source)
target_compile_options(Locutus PRIVATE -w)
