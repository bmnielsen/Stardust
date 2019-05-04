
include_directories(
  ..
  ../../Util/Source
  ../../BWAPI/openbw
  ../../BWAPI/openbw/BWAPI
  ../../BWAPI/Source/BWAPI
  ../../BWAPI/Source
  ../../BWAPICore
  ../../Shared
)

add_definitions(-DCOMPAT)
add_definitions(-DCOMPAT_3_7_4)

add_executable(BWAPILauncher-3.7.4
  ../../BWAPILauncher/Source/Main.cpp
)

target_link_libraries(BWAPILauncher-3.7.4 BWAPI-3.7.4)

if (NOT WIN32)
  target_link_libraries(BWAPILauncher-3.7.4 dl)
endif()

install(TARGETS BWAPILauncher-3.7.4
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
