cmake_minimum_required(VERSION 3.14)
project(Steamhammer)

set(CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/Steamhammer/Source/*.cpp)
list(FILTER SRC_FILES EXCLUDE REGEX Dll.cpp)

file(GLOB_RECURSE BOSS_SRC_FILES ${PROJECT_SOURCE_DIR}/BOSS/source/*.cpp)
list(FILTER BOSS_SRC_FILES EXCLUDE REGEX BOSS_main.cpp)
list(FILTER BOSS_SRC_FILES EXCLUDE REGEX StarCraftGUI.cpp)
list(FILTER BOSS_SRC_FILES EXCLUDE REGEX deprecated)

add_library(Steamhammer SHARED ${SRC_FILES} ${BOSS_SRC_FILES})

target_link_libraries(Steamhammer -L/usr/local/Cellar/llvm/8.0.0/lib BWAPILIB)
target_include_directories(Steamhammer SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/Steamhammer/Source)
target_compile_options(Steamhammer PRIVATE -w)
