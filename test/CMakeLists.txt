cmake_minimum_required(VERSION 3.14)
project(tests)

add_subdirectory(3rdparty/googletest)
add_subdirectory(3rdparty/opponents/Steamhammer)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty/googletest/include)

include_directories(
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/Util/Source
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/openbw
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/openbw/BWAPI
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/Source/BWAPI
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/Source
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPICore
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/OpenBWData
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/Shared
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/include
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/openbw
)

#add_library(BWData ALIAS OpenBWData)

add_executable(tests
        $<TARGET_OBJECTS:BWAPIObj>
        $<TARGET_OBJECTS:BWAPICore>
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/OpenBWData/BW/BWData.cpp
        BWTest.cpp
        SelfPlay.cpp
        Steamhammer.cpp
        AiideMapAnalysis.cpp)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(tests PUBLIC DEBUG=1)
endif()

target_link_libraries(tests gtest gtest_main BWAPILIB Locutus Steamhammer)

file(COPY ${CMAKE_SOURCE_DIR}/test/maps DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/opponents/Steamhammer/Steamhammer_2.3.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
