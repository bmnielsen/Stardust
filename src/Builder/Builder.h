#pragma once

#include "Common.h"

#include "Building.h"
#include "MyWorker.h"

class Base;

namespace Constructor
{
    void initialize();

    void update();

    void issueOrders();

    void build(BWAPI::UnitType type, BWAPI::TilePosition tilePos, const OwnedWorker &builder, int startFrame = 0);

    void cancel(BWAPI::TilePosition tilePos);

    OwnedWorker getBuilderUnit(BWAPI::TilePosition tilePos, BWAPI::UnitType type, int *expectedArrivalFrame = nullptr);

    std::vector<std::shared_ptr<Building>> &allPendingBuildings();

    std::vector<Building *> pendingBuildingsOfType(BWAPI::UnitType type);

    void cancelBase(Base *baseLoc);

    bool isPendingHere(BWAPI::TilePosition tilePos);

    Building *pendingHere(BWAPI::TilePosition tilePos);

    bool hasPendingBuilding(const OwnedWorker &builder);

    void addReservedBuilder(const OwnedWorker &builder);

    void releaseReservedBuilder(const OwnedWorker &builder);

    bool isInEnemyStaticThreatRange(BWAPI::TilePosition tilePos, BWAPI::UnitType type);

    /* If there is a building at this tile, gets the frames until it is completed. */
    /* If there is already a completed building, returns 0. */
    /* If there is no building pending or completed, returns the given default value. */
    int framesUntilCompleted(BWAPI::TilePosition tilePos, int defaultValue);
}
