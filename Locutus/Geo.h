#pragma once

#include "Common.h"

namespace Geo
{
    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);
    bool Overlaps(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
    bool Overlaps(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);
    bool Walkable(BWAPI::UnitType type, BWAPI::Position center);
    BWAPI::Position FindClosestUnwalkablePosition(BWAPI::Position start, BWAPI::Position closeTo, int searchRadius);
    void FindWalkablePositionsBetween(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> & result);
}