
include_directories(
  ../Util/Source
  ../BWAPI/openbw
  ../BWAPI/openbw/BWAPI
  ../BWAPI/Source/BWAPI
  ../BWAPI/Source
  ../BWAPICore
  ../Shared
)

add_library(BWAPIClient STATIC
  Source/Client.cpp
)

target_link_libraries(BWAPIClient BWAPI)

set_property(TARGET BWAPIClient PROPERTY POSITION_INDEPENDENT_CODE ON)

install(TARGETS BWAPIClient
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
