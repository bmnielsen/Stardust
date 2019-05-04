
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
add_definitions(-DCOMPAT_4_1_2)

add_executable(BWAPILauncher-4.1.2
  ../../BWAPILauncher/Source/Main.cpp
)

target_link_libraries(BWAPILauncher-4.1.2 BWAPI-4.1.2)

if (NOT WIN32)
  target_link_libraries(BWAPILauncher-4.1.2 dl)
endif()

install(TARGETS BWAPILauncher-4.1.2
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
