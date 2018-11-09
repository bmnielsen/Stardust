#pragma once

#include "Common.h"

namespace BuildingPlacer
{
    void initialize();

    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitMorph(BWAPI::Unit unit);
    void onUnitCreate(BWAPI::Unit unit);
    void onUnitDiscover(BWAPI::Unit unit);

    // Get a building location for the given type near the given location.
    // A set of reserved positions may be provided if you are in the process of finding locations
    // for multiple buildings and need to ensure they do not overlap.
    BWAPI::TilePosition getBuildingLocation(
        BWAPI::UnitType type,
        BWAPI::TilePosition nearTile,
        std::set<BWAPI::TilePosition> * reservedTiles = nullptr);
}