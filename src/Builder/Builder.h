#pragma once

#include "Common.h"

#include "Building.h"
#include "MyUnit.h"

namespace Builder
{
    void initialize();

    void update();

    void issueOrders();

    void build(BWAPI::UnitType type, BWAPI::TilePosition tile, MyUnit builder, int startFrame = 0);

    void cancel(BWAPI::TilePosition tile);

    MyUnit getBuilderUnit(BWAPI::TilePosition tile, BWAPI::UnitType type, int *expectedArrivalFrame = nullptr);

    std::vector<std::shared_ptr<Building>> &allPendingBuildings();

    std::vector<Building *> pendingBuildingsOfType(BWAPI::UnitType type);

    bool isPendingHere(BWAPI::TilePosition tile);

    Building *pendingHere(BWAPI::TilePosition tile);

    bool hasPendingBuilding(const MyUnit &builder);

    void addReservedBuilder(const MyUnit &builder);

    void releaseReservedBuilder(const MyUnit &builder);
}
