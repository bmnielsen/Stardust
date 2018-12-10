#pragma once

#include "Common.h"

namespace UnitUtil
{
    bool IsUndetected(BWAPI::Unit unit);

    BWAPI::Position PredictPosition(BWAPI::Unit unit, int frames);

    bool Powers(BWAPI::TilePosition pylonTile, BWAPI::TilePosition buildingTile, BWAPI::UnitType buildingType);

    int BuildTime(BWAPI::UnitType type);
}