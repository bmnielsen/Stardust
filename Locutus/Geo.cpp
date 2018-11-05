#include "Geo.h"

#include <set>

namespace Geo
{
    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter)
    {
        // Compute bounding boxes
        BWAPI::Position firstTopLeft = firstCenter + BWAPI::Position(-firstType.dimensionLeft(), -firstType.dimensionUp());
        BWAPI::Position firstBottomRight = firstCenter + BWAPI::Position(firstType.dimensionRight(), firstType.dimensionDown());
        BWAPI::Position secondTopLeft = secondCenter + BWAPI::Position(-secondType.dimensionLeft(), -secondType.dimensionUp());
        BWAPI::Position secondBottomRight = secondCenter + BWAPI::Position(secondType.dimensionRight(), secondType.dimensionDown());

        // Compute offsets
        int xDist = (std::max)({ firstTopLeft.x - secondBottomRight.x - 1, secondTopLeft.x - firstBottomRight.x - 1, 0 });
        int yDist = (std::max)({ firstTopLeft.y - secondBottomRight.y - 1, secondTopLeft.y - firstBottomRight.y - 1, 0 });

        // Compute distance
        return BWAPI::Positions::Origin.getApproxDistance(BWAPI::Position(xDist, yDist));
    }

    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point)
    {
        // Compute bounding box
        BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
        BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

        // Compute offsets
        int xDist = (std::max)({ topLeft.x - point.x - 1, point.x - bottomRight.x - 1, 0 });
        int yDist = (std::max)({ topLeft.y - point.y - 1, point.y - bottomRight.y - 1, 0 });

        // Compute distance
        return BWAPI::Positions::Origin.getApproxDistance(BWAPI::Position(xDist, yDist));
    }

    bool Overlaps(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter)
    {
        // Compute bounding boxes
        BWAPI::Position firstTopLeft = firstCenter + BWAPI::Position(-firstType.dimensionLeft(), -firstType.dimensionUp());
        BWAPI::Position firstBottomRight = firstCenter + BWAPI::Position(firstType.dimensionRight(), firstType.dimensionDown());
        BWAPI::Position secondTopLeft = secondCenter + BWAPI::Position(-secondType.dimensionLeft(), -secondType.dimensionUp());
        BWAPI::Position secondBottomRight = secondCenter + BWAPI::Position(secondType.dimensionRight(), secondType.dimensionDown());

        return firstBottomRight.x >= secondTopLeft.x && secondBottomRight.x >= firstTopLeft.x &&
            firstBottomRight.y >= secondTopLeft.y && secondBottomRight.y >= firstTopLeft.y;
    }

    bool Overlaps(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point)
    {
        // Compute bounding box
        BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
        BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

        return bottomRight.x >= point.x && point.x >= topLeft.x &&
            bottomRight.y >= point.y && point.y >= topLeft.y;
    }

    bool Walkable(BWAPI::UnitType type, BWAPI::Position center)
    {
        for (int x = center.x - type.dimensionLeft(); x <= center.x + type.dimensionRight(); x++)
            for (int y = center.y - type.dimensionUp(); y <= center.y + type.dimensionDown(); y++)
                if (!BWAPI::Broodwar->isWalkable(x / 8, y / 8))
                    return false;
        return true;
    }

    BWAPI::Position Geo::FindClosestUnwalkablePosition(BWAPI::Position start, BWAPI::Position closeTo, int searchRadius)
    {
        BWAPI::Position bestPos = BWAPI::Positions::Invalid;
        int bestDist = INT_MAX;
        for (int x = start.x - searchRadius; x <= start.x + searchRadius; x++)
            for (int y = start.y - searchRadius; y <= start.y + searchRadius; y++)
            {
                BWAPI::Position current(x, y);
                if (!current.isValid()) continue;
                if (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(current))) continue;
                int dist = current.getDistance(closeTo);
                if (dist < bestDist)
                {
                    bestPos = current;
                    bestDist = dist;
                }
            }

        return bestPos;
    }

    void Geo::FindWalkablePositionsBetween(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> & result)
    {
        std::set<BWAPI::Position> added;
        int distTotal = std::round(start.getDistance(end));
        int xdiff = end.x - start.x;
        int ydiff = end.y - start.y;
        for (int distStop = 0; distStop <= distTotal; distStop++)
        {
            BWAPI::Position pos(
                start.x + std::round(((double)distStop / distTotal) * xdiff),
                start.y + std::round(((double)distStop / distTotal) * ydiff));

            if (!pos.isValid()) continue;
            if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))) continue;
            if (added.find(pos) != added.end()) continue;

            result.push_back(pos);
            added.insert(pos);
        }
    }
}
