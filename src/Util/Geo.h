#pragma once

#include "Common.h"

namespace Geo
{
    void initialize();

    int ApproximateDistance(int x1, int x2, int y1, int y2);

    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);

    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);

    int EdgeToTileDistance(BWAPI::UnitType type, BWAPI::TilePosition topLeft, BWAPI::TilePosition tile);

    BWAPI::Position NearestPointOnEdge(BWAPI::Position point, BWAPI::UnitType type, BWAPI::Position center);

    bool Overlaps(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);

    bool Overlaps(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);

    bool Overlaps(BWAPI::TilePosition firstTopLeft, int firstWidth, int firstHeight,
                  BWAPI::TilePosition secondtopLeft, int secondWidth, int secondHeight);

    bool Walkable(BWAPI::UnitType type, BWAPI::Position center);

    BWAPI::Position FindClosestUnwalkablePosition(BWAPI::Position start,
                                                  int searchRadius,
                                                  BWAPI::Position furtherFrom = BWAPI::Positions::Invalid);

    BWAPI::Position FindClosestUnwalkablePosition(BWAPI::Position start,
                                                  BWAPI::Position closeTo,
                                                  int searchRadius,
                                                  BWAPI::Position furtherFrom = BWAPI::Positions::Invalid);

    void FindWalkablePositionsBetween(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> &result);

    void FindTilesBetween(BWAPI::TilePosition start, BWAPI::TilePosition end, std::vector<BWAPI::TilePosition> &result);

    void FindWalkTilesBetween(BWAPI::WalkPosition start, BWAPI::WalkPosition end, std::vector<BWAPI::WalkPosition> &result);

    BWAPI::Position CenterOfUnit(BWAPI::TilePosition topLeft, BWAPI::UnitType type);

    BWAPI::Position CenterOfUnit(BWAPI::Position topLeft, BWAPI::UnitType type);

    BWAPI::Position ScaleVector(BWAPI::Position vector, int length);

    BWAPI::Position PerpendicularVector(BWAPI::Position vector, int length);

    enum Direction { up, down, left, right, upleft, downleft, upright, downright, error };

    // Gets the relative direction between a building and a position
    Direction directionFromBuilding(BWAPI::TilePosition tile, BWAPI::TilePosition size, BWAPI::Position pos, bool fourDirections = false);

    // The direction along a vector in BW representation (1/256th of a circle)
    int BWDirection(BWAPI::Position vector);

    // Difference between two angles in BW representation (1/256th of a circle)
    int BWAngleDiff(int a, int b);

    // Adds twho angles in BW representation (1/256th of a circle)
    int BWAngleAdd(int a, int b);

    // Simulate a frame of movement of a unit
    // All values are in BW representation (positions and speeds are multiples of 1/256, angles are 1/256th of a circle)
    void BWMovement(int &x, int &y, int &heading, int desiredHeading, int turnRate, int &speed, int acceleration, int topSpeed);

    class Spiral
    {
    public:
        int x;
        int y;
        int radius;

        Spiral()
                : x(0)
                , y(0)
                , radius(1)
                , direction(0) {}

        void Next()
        {
            switch (direction)
            {
                case 0:
                    ++x;
                    if (x == radius) ++direction;
                    break;
                case 1:
                    ++y;
                    if (y == radius) ++direction;
                    break;
                case 2:
                    --x;
                    if (-x == radius) ++direction;
                    break;
                case 3:
                    --y;
                    if (-y == radius)
                    {
                        direction = 0;
                        ++radius;
                    }
                    break;
            }
        }

    private:
        unsigned int direction;
    };
}