cmake_minimum_required(VERSION 3.24)
project(Stardust)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty)

set(STARDUST_INCLUDES
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
include_directories(${STARDUST_INCLUDES})

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR} *.cpp)

add_library(Stardust SHARED ${SRC_FILES})

IF (VS_HUMAN)
    target_compile_definitions(Stardust PUBLIC VS_HUMAN=true)
ENDIF (VS_HUMAN)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(Stardust PUBLIC DEBUG=1)
endif ()

IF (MSVC)
    IF (TOURNAMENT_BUILD)
        target_link_libraries(Stardust BWAPILIB BWEM FAP nlohmann)
    ELSE (TOURNAMENT_BUILD)
        target_compile_definitions(Stardust PUBLIC LOGGING_ENABLED=true)

        if (NOT CMAKE_BUILD_TYPE MATCHES Release)
            target_compile_definitions(Stardust PUBLIC INSTRUMENTATION_ENABLED=true)
        endif ()

        target_link_libraries(Stardust BWAPILIB BWEM FAP nlohmann libzstd_static)
    ENDIF (TOURNAMENT_BUILD)

ELSE (MSVC)
    target_compile_definitions(Stardust PUBLIC IS_OPENBW=true)

    if (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined,leak -fsanitize-recover=address,undefined,leak -g")
    endif()

    if (CMAKE_BUILD_TYPE MATCHES Release)
        target_link_libraries(Stardust BWAPILIB BWEM FAP nlohmann)
    else ()
        target_compile_definitions(Stardust PUBLIC LOGGING_ENABLED=true)
        target_compile_definitions(Stardust PUBLIC INSTRUMENTATION_ENABLED=true)
        target_compile_definitions(Stardust PUBLIC INSTRUMENTATION_ENABLED_VERBOSE=true)
        target_link_libraries(Stardust BWAPILIB BWEM FAP nlohmann libzstd_static)
    endif()
ENDIF (MSVC)

target_include_directories(Stardust PUBLIC ${STARDUST_INCLUDES})

IF (NOT MSVC)
    target_compile_options(Stardust PRIVATE -Wall -Werror)
ENDIF (NOT MSVC)
