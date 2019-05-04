include_directories(
  ../include 
)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(TestAIModule SHARED
  Source/AttackMoveTest.cpp
  Source/AttackUnitTest.cpp
  Source/BuildingPlacer.cpp
  Source/BuildTest.cpp
  Source/BurrowTest.cpp
  Source/BWAssert.cpp
  Source/CancelConstructionTest.cpp
  Source/CancelMorphTest.cpp
  Source/CancelResearchTest.cpp
  Source/CancelTrainTest.cpp
  Source/CancelUpgradeTest.cpp
  Source/CloakTest.cpp
  Source/DefaultTestModule.cpp
  Source/Dll.cpp
  Source/EventTest.cpp
  Source/ExistenceTest.cpp
  Source/FollowTest.cpp
  Source/GatherTest.cpp
  Source/InterceptorTest.cpp
  Source/LiftTest.cpp
  Source/MapTest.cpp
  Source/MicroTest.cpp
  Source/MorphTest.cpp
  Source/MPHSTest.cpp
  Source/ProtossTest.cpp
  Source/RallyTest.cpp
  Source/RepairTest.cpp
  Source/ResearchTest.cpp
  Source/RightClickTest.cpp
  Source/SiegeTest.cpp
  Source/TerranTest.cpp
  Source/TestMap1.cpp
  Source/TestModule.cpp
  Source/TrainTest.cpp
  Source/TransportTest.cpp
  Source/UpgradeTest.cpp
  Source/UseTechTest.cpp
  Source/ZergTest.cpp
  )

target_link_libraries(TestAIModule BWAPILIB)
