
include_directories(
  ../Util/Source
  ./Source/BWAPI
  ./Source
  ../BWAPICore
  ../Shared
  ../OpenBWData
)

add_definitions(-DDEBUG_NEW)

add_library(BWAPIObj OBJECT
  Source/Config.cpp
  Source/WMode.cpp
  Source/GameUpdate.cpp
  Source/Graphics.cpp
  Source/BW/OrderTypes.cpp
  Source/BW/UnitTarget.cpp
  Source/BW/PositionUnitTarget.cpp
  Source/BWAPI/GameInternals.cpp
  Source/BWAPI/PlayerImpl.cpp
  Source/BWAPI/GameImpl.cpp
  Source/BWAPI/Server.cpp
  Source/BWAPI/UnitImpl.cpp
  Source/BWAPI/CommandOptimizer.cpp
  Source/BWAPI/BulletImpl.cpp
  Source/BWAPI/GameMenu.cpp
  Source/BWAPI/AutoMenuManager.cpp
  Source/BWAPI/Map.cpp
  Source/BWAPI/GameCommands.cpp
  Source/BWAPI/GameEvents.cpp
  Source/BWAPI/GameBullets.cpp
  Source/BWAPI/UnitUpdate.cpp
  Source/BWAPI/GameUnits.cpp
  Source/BWAPI/GameDrawing.cpp
  Source/BWAPI/RegionImpl.cpp
  Source/BWAPI/ForceImpl.cpp
  Source/BW/CheatType.cpp
  Source/BWAPI/BWtoBWAPI.cpp
  Source/BW/Bitmap.cpp
  ../Shared/GameShared.cpp
  ../Shared/PlayerShared.cpp
  ../Shared/UnitShared.cpp
  ../Shared/BulletShared.cpp
  ../Shared/RegionShared.cpp
  ../Shared/ForceShared.cpp
  ../Util/Source/Util/Sha1.cpp
)

set_property(TARGET BWAPIObj PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(BWAPI SHARED
  $<TARGET_OBJECTS:BWAPIObj>
  $<TARGET_OBJECTS:BWAPICore>
)

target_link_libraries(BWAPI BWAPILIB BWData)

set_property(TARGET BWAPI PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)

install(TARGETS BWAPI
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
