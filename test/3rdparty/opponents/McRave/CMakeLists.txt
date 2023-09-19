cmake_minimum_required(VERSION 3.24)
project(McRave)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/McRave/*.cpp)
file(GLOB_RECURSE BWEM_SRC_FILES ${PROJECT_SOURCE_DIR}/BWEM/*.cpp)
file(GLOB_RECURSE BWEB_SRC_FILES ${PROJECT_SOURCE_DIR}/BWEB/*.cpp)
file(GLOB_RECURSE HORIZON_SRC_FILES ${PROJECT_SOURCE_DIR}/Horizon/*.cpp)

set(MCRAVE_INCLUDES
        ${PROJECT_SOURCE_DIR}/BWEM
        ${PROJECT_SOURCE_DIR}/BWEB
        ${PROJECT_SOURCE_DIR}/HORIZON
        )
include_directories(${MCRAVE_INCLUDES})

add_library(McRave SHARED ${SRC_FILES} ${BWEM_SRC_FILES} ${BWEB_SRC_FILES} ${HORIZON_SRC_FILES})

target_link_libraries(McRave BWAPILIB)
target_include_directories(McRave SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/McRave)
target_compile_options(McRave PRIVATE -w)
