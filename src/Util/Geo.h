#pragma once

#include "Common.h"

namespace Geo
{
    int ApproximateDistance(int x1, int x2, int y1, int y2);

    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);

    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);

    BWAPI::Position NearestPointOnEdge(BWAPI::Position point, BWAPI::UnitType type, BWAPI::Position center);

    bool Overlaps(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);

    bool Overlaps(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);

    bool Overlaps(BWAPI::TilePosition firstTopLeft, int firstWidth, int firstHeight,
                  BWAPI::TilePosition secondtopLeft, int secondWidth, int secondHeight);

    bool Walkable(BWAPI::UnitType type, BWAPI::Position center);

    BWAPI::Position FindClosestUnwalkablePosition(BWAPI::Position start,
                                                  BWAPI::Position closeTo,
                                                  int searchRadius,
                                                  BWAPI::Position furtherFrom = BWAPI::Positions::Invalid);

    void FindWalkablePositionsBetween(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> &result);

    void FindTilesBetween(BWAPI::TilePosition start, BWAPI::TilePosition end, std::vector<BWAPI::TilePosition> &result);

    BWAPI::Position CenterOfUnit(BWAPI::TilePosition topLeft, BWAPI::UnitType type);

    BWAPI::Position CenterOfUnit(BWAPI::Position topLeft, BWAPI::UnitType type);

    BWAPI::Position ScaleVector(BWAPI::Position vector, int length);

}