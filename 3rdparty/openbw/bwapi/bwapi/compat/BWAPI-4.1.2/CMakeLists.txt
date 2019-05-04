
include_directories(
  ..
  ../../Util/Source
  ../../BWAPI/Source/BWAPI
  ../../BWAPI/Source
  ../../BWAPICore
  ../../Shared
)

add_definitions(-DCOMPAT)
add_definitions(-DCOMPAT_4_1_2)

add_library(BWAPIObj-4.1.2 OBJECT
  ../CompatGameImpl.cpp
  ../../BWAPI/Source/Config.cpp
  ../../BWAPI/Source/WMode.cpp
  ../../BWAPI/Source/GameUpdate.cpp
  ../../BWAPI/Source/Graphics.cpp
  ../../BWAPI/Source/BW/OrderTypes.cpp
  ../../BWAPI/Source/BW/UnitTarget.cpp
  ../../BWAPI/Source/BW/PositionUnitTarget.cpp
  ../../BWAPI/Source/BWAPI/GameInternals.cpp
  ../../BWAPI/Source/BWAPI/PlayerImpl.cpp
  ../../BWAPI/Source/BWAPI/GameImpl.cpp
  ../../BWAPI/Source/BWAPI/Server.cpp
  ../../BWAPI/Source/BWAPI/UnitImpl.cpp
  ../../BWAPI/Source/BWAPI/CommandOptimizer.cpp
  ../../BWAPI/Source/BWAPI/BulletImpl.cpp
  ../../BWAPI/Source/BWAPI/GameMenu.cpp
  ../../BWAPI/Source/BWAPI/AutoMenuManager.cpp
  ../../BWAPI/Source/BWAPI/Map.cpp
  ../../BWAPI/Source/BWAPI/GameCommands.cpp
  ../../BWAPI/Source/BWAPI/GameEvents.cpp
  ../../BWAPI/Source/BWAPI/GameBullets.cpp
  ../../BWAPI/Source/BWAPI/UnitUpdate.cpp
  ../../BWAPI/Source/BWAPI/GameUnits.cpp
  ../../BWAPI/Source/BWAPI/GameDrawing.cpp
  ../../BWAPI/Source/BWAPI/RegionImpl.cpp
  ../../BWAPI/Source/BWAPI/ForceImpl.cpp
  ../../BWAPI/Source/BW/CheatType.cpp
  ../../BWAPI/Source/BWAPI/BWtoBWAPI.cpp
  ../../BWAPI/Source/BW/Bitmap.cpp
  ../../Shared/GameShared.cpp
  ../../Shared/PlayerShared.cpp
  ../../Shared/UnitShared.cpp
  ../../Shared/BulletShared.cpp
  ../../Shared/RegionShared.cpp
  ../../Shared/ForceShared.cpp
  ../../Util/Source/Util/Sha1.cpp
)

set_property(TARGET BWAPIObj-4.1.2 PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(BWAPI-4.1.2 SHARED
  $<TARGET_OBJECTS:BWAPIObj-4.1.2>
  $<TARGET_OBJECTS:BWAPICore>
)

target_link_libraries(BWAPI-4.1.2 BWAPILIB BWData)

set_property(TARGET BWAPI-4.1.2 PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)

install(TARGETS BWAPI-4.1.2
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
