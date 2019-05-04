
include_directories(
  ../Util/Source
  ../BWAPI/openbw
  ../BWAPI/openbw/BWAPI
  ../BWAPI/Source/BWAPI
  ../BWAPI/Source
  ../BWAPICore
  ../Shared
)

add_executable(BWAPILauncher
  Source/Main.cpp
)

target_link_libraries(BWAPILauncher BWAPI)

if (NOT WIN32)
  target_link_libraries(BWAPILauncher dl)
endif()

install(TARGETS BWAPILauncher
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
