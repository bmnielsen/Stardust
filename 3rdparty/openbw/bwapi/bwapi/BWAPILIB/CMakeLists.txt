
set(BWAPILIB_SRCS
  UnitCommand.cpp
  Source/AIModule.cpp
  Source/BroodwarOutputDevice.cpp
  Source/BulletType.cpp
  Source/BWAPI.cpp
  Source/Color.cpp
  Source/DamageType.cpp
  Source/Error.cpp
  Source/Event.cpp
  Source/ExplosionType.cpp
  Source/Filters.cpp
  Source/Forceset.cpp
  Source/Game.cpp
  Source/GameType.cpp
  Source/Order.cpp
  Source/Player.cpp
  Source/Playerset.cpp
  Source/PlayerType.cpp
  Source/Position.cpp
  Source/Race.cpp
  Source/Region.cpp
  Source/Regionset.cpp
  Source/Streams.cpp
  Source/TechType.cpp
  Source/Unit.cpp
  Source/Unitset.cpp
  Source/UnitCommandType.cpp
  Source/UnitSizeType.cpp
  Source/UnitType.cpp
  Source/UpgradeType.cpp
  Source/WeaponType.cpp
)

if (WIN32)
  add_library(BWAPILIB STATIC
    ${BWAPILIB_SRCS}
  )
else()
  add_library(BWAPILIB SHARED
    ${BWAPILIB_SRCS}
  )
endif()

set_property(TARGET BWAPILIB PROPERTY POSITION_INDEPENDENT_CODE ON)

install(TARGETS BWAPILIB
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

