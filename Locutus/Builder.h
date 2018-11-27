#pragma once

#include "Common.h"

#include "Building.h"

namespace Builder
{
    void update();

    void build(BWAPI::UnitType type, BWAPI::TilePosition tile, BWAPI::Unit builder);

    BWAPI::Unit getBuilderUnit(BWAPI::TilePosition tile, BWAPI::UnitType type, int * expectedArrivalFrame = nullptr);
    
    std::vector<std::shared_ptr<Building>> allPendingBuildings();
    std::vector<std::shared_ptr<Building>> pendingBuildingsOfType(BWAPI::UnitType type);
    bool isPendingHere(BWAPI::TilePosition tile);
}
