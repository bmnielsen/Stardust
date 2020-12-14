cmake_minimum_required(VERSION 3.14)
project(tests)

add_subdirectory(3rdparty/googletest)
add_subdirectory(3rdparty/opponents/Steamhammer)
add_subdirectory(3rdparty/opponents/Locutus)
add_subdirectory(3rdparty/opponents/Iron)
add_subdirectory(3rdparty/opponents/BananaBrain)
add_subdirectory(3rdparty/opponents/SAIDA)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty/googletest/include)

include_directories(SYSTEM
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/Util/Source
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/openbw
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/openbw/BWAPI
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/Source
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPICore
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/OpenBWData
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/Shared
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/openbw
)

set(STARDUST_TEST_INCLUDES
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/Infrastructure
        ${PROJECT_SOURCE_DIR}/Infrastructure/Modules
        ${PROJECT_SOURCE_DIR}/Infrastructure/Plays
        ${PROJECT_SOURCE_DIR}/Infrastructure/StrategyEngines
        )
include_directories(${STARDUST_TEST_INCLUDES})

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR} *.cpp)
list(FILTER SRC_FILES EXCLUDE REGEX ".*3rdparty.*")

add_executable(tests
        $<TARGET_OBJECTS:BWAPIObj>
        $<TARGET_OBJECTS:BWAPICore>
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/OpenBWData/BW/BWData.cpp
        ${SRC_FILES}
        )

set_source_files_properties(
        $<TARGET_OBJECTS:BWAPIObj>
        $<TARGET_OBJECTS:BWAPICore>
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/OpenBWData/BW/BWData.cpp
        PROPERTIES
        COMPILE_FLAGS "-w"
)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(tests PUBLIC DEBUG=1)
endif()

target_link_libraries(tests gtest gtest_main BWAPILIB Stardust Steamhammer Locutus Iron BananaBrain SAIDA)
target_compile_options(tests PRIVATE -Wall -Werror)

file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/maps DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/bwta DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/opponents/Steamhammer/Steamhammer_2.4.2.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/opponents/Locutus/Locutus.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
