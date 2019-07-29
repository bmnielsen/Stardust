#pragma once

#include "Common.h"

namespace Geo
{
    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
    int EdgeToEdgeSquaredDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);
    BWAPI::Position NearestPointOnEdge(BWAPI::Position point, BWAPI::UnitType type, BWAPI::Position center);
    bool Overlaps(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
    bool Overlaps(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);
    bool Walkable(BWAPI::UnitType type, BWAPI::Position center);
    BWAPI::Position FindClosestUnwalkablePosition(BWAPI::Position start, BWAPI::Position closeTo, int searchRadius);
    void FindWalkablePositionsBetween(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> & result);
    BWAPI::Position FindInterceptPoint(BWAPI::Unit interceptor, BWAPI::Unit target);
}