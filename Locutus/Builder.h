#pragma once

#include "Common.h"

#include "Building.h"

namespace Builder
{
    void update();
    void issueOrders();

    void build(BWAPI::UnitType type, BWAPI::TilePosition tile, BWAPI::Unit builder);

    BWAPI::Unit getBuilderUnit(BWAPI::TilePosition tile, BWAPI::UnitType type, int * expectedArrivalFrame = nullptr);
    
    std::vector<Building*> allPendingBuildings();
    std::vector<Building*> pendingBuildingsOfType(BWAPI::UnitType type);
    bool isPendingHere(BWAPI::TilePosition tile);
}
