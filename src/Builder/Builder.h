#pragma once

#include "Common.h"

#include "Building.h"
#include "MyUnit.h"

class Base;

namespace Builder
{
    void initialize();

    void update();

    void issueOrders();

    void build(BWAPI::UnitType type, BWAPI::TilePosition tile, const MyUnit &builder, int startFrame = 0);

    void cancel(BWAPI::TilePosition tile);

    MyUnit getBuilderUnit(BWAPI::TilePosition tile, BWAPI::UnitType type, int *expectedArrivalFrame = nullptr);

    std::vector<std::shared_ptr<Building>> &allPendingBuildings();

    std::vector<Building *> pendingBuildingsOfType(BWAPI::UnitType type);

    void cancelBase(Base *base);

    bool isPendingHere(BWAPI::TilePosition tile);

    Building *pendingHere(BWAPI::TilePosition tile);

    bool hasPendingBuilding(const MyUnit &builder);

    void addReservedBuilder(const MyUnit &builder);

    void releaseReservedBuilder(const MyUnit &builder);

    bool isInEnemyStaticThreatRange(BWAPI::TilePosition tile, BWAPI::UnitType type);

    // If there is a building at this tile, gets the frames until it is completed.
    // If there is already a completed building, returns 0.
    // If there is no building pending or completed, returns the given default value.
    int framesUntilCompleted(BWAPI::TilePosition tile, int defaultValue);
}
