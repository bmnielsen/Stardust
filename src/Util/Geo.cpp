#include "Geo.h"

#include <set>

namespace Geo
{
    int ApproximateDistance(int x1, int x2, int y1, int y2)
    {
        unsigned int min = abs((int) (x1 - x2));
        unsigned int max = abs((int) (y1 - y2));
        if (max < min)
            std::swap(min, max);

        if (min < (max >> 2U))
            return max;

        unsigned int minCalc = (3 * min) >> 3U;
        return (minCalc >> 5U) + minCalc + max - (max >> 4U) - (max >> 6U);
    }

    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter)
    {
        // Compute bounding boxes
        BWAPI::Position firstTopLeft = firstCenter + BWAPI::Position(-firstType.dimensionLeft(), -firstType.dimensionUp());
        BWAPI::Position firstBottomRight = firstCenter + BWAPI::Position(firstType.dimensionRight(), firstType.dimensionDown());
        BWAPI::Position secondTopLeft = secondCenter + BWAPI::Position(-secondType.dimensionLeft(), -secondType.dimensionUp());
        BWAPI::Position secondBottomRight = secondCenter + BWAPI::Position(secondType.dimensionRight(), secondType.dimensionDown());

        // Compute offsets
        int xDist = (std::max)({firstTopLeft.x - secondBottomRight.x - 1, secondTopLeft.x - firstBottomRight.x - 1, 0});
        int yDist = (std::max)({firstTopLeft.y - secondBottomRight.y - 1, secondTopLeft.y - firstBottomRight.y - 1, 0});

        // Compute distance
        return ApproximateDistance(xDist, 0, yDist, 0);
    }

    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point)
    {
        // Compute bounding box
        BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
        BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

        // Compute offsets
        int xDist = (std::max)({topLeft.x - point.x - 1, point.x - bottomRight.x - 1, 0});
        int yDist = (std::max)({topLeft.y - point.y - 1, point.y - bottomRight.y - 1, 0});

        // Compute distance
        return ApproximateDistance(xDist, 0, yDist, 0);
    }

    BWAPI::Position NearestPointOnEdge(BWAPI::Position point, BWAPI::UnitType type, BWAPI::Position center)
    {
        // Compute bounding box
        BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
        BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

        return BWAPI::Position(
                point.x < topLeft.x ? topLeft.x : (point.x > bottomRight.x ? bottomRight.x : point.x),
                point.y < topLeft.y ? topLeft.y : (point.y > bottomRight.y ? bottomRight.y : point.y));
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
        {
            for (int y = center.y - type.dimensionUp(); y <= center.y + type.dimensionDown(); y++)
            {
                if (!BWAPI::Broodwar->isWalkable(x / 8, y / 8))
                    return false;
            }
        }
        return true;
    }

    BWAPI::Position FindClosestUnwalkablePosition(BWAPI::Position start, BWAPI::Position closeTo, int searchRadius)
    {
        BWAPI::Position bestPos = BWAPI::Positions::Invalid;
        int bestDist = INT_MAX;
        for (int x = start.x - searchRadius; x <= start.x + searchRadius; x++)
        {
            for (int y = start.y - searchRadius; y <= start.y + searchRadius; y++)
            {
                BWAPI::Position current(x, y);
                if (!current.isValid()) continue;
                if (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(current))) continue;
                int dist = current.getApproxDistance(closeTo);
                if (dist < bestDist)
                {
                    bestPos = current;
                    bestDist = dist;
                }
            }
        }

        return bestPos;
    }

    void FindWalkablePositionsBetween(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> &result)
    {
        std::set<BWAPI::Position> added;
        int distTotal = start.getApproxDistance(end);
        int xdiff = end.x - start.x;
        int ydiff = end.y - start.y;
        for (int distStop = 0; distStop <= distTotal; distStop++)
        {
            BWAPI::Position pos(
                    start.x + (int) std::round(((double) distStop / distTotal) * xdiff),
                    start.y + (int) std::round(((double) distStop / distTotal) * ydiff));

            if (!pos.isValid()) continue;
            if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))) continue;
            if (added.find(pos) != added.end()) continue;

            result.push_back(pos);
            added.insert(pos);
        }
    }

    void FindTilesBetween(BWAPI::TilePosition start, BWAPI::TilePosition end, std::vector<BWAPI::TilePosition> &result)
    {
        std::set<BWAPI::TilePosition> added;
        int distTotal = start.getApproxDistance(end);
        int xdiff = end.x - start.x;
        int ydiff = end.y - start.y;
        for (int distStop = 0; distStop <= distTotal; distStop++)
        {
            BWAPI::TilePosition pos(
                    start.x + (int) std::round(((double) distStop / distTotal) * xdiff),
                    start.y + (int) std::round(((double) distStop / distTotal) * ydiff));

            if (!pos.isValid()) continue;
            if (added.find(pos) != added.end()) continue;

            result.push_back(pos);
            added.insert(pos);
        }
    }

    // Computes the intercept point of a unit targeting another one, assuming the interceptor
    // is going at full speed and the target remains on its current trajectory and speed.
    // Returns invalid if the target cannot be intercepted.
    // Uses the formula derived here: http://jaran.de/goodbits/2011/07/17/calculating-an-intercept-course-to-a-target-with-constant-direction-and-velocity-in-a-2-dimensional-plane/
    BWAPI::Position FindInterceptPoint(BWAPI::Unit interceptor, BWAPI::Unit target)
    {
        double speed = interceptor->getType().topSpeed();

        double diffX = (double) target->getPosition().x - interceptor->getPosition().x;
        double diffY = (double) target->getPosition().y - interceptor->getPosition().y;

        double diffSpeed = target->getVelocityX() * target->getVelocityX() + target->getVelocityY() * target->getVelocityY() - speed * speed;
        double diffDist = diffX * target->getVelocityX() + diffY * target->getVelocityY();

        double t;
        if (diffSpeed < 0.0001)
        {
            t = -(diffX * diffX + diffY * diffY) / (2 * diffDist);
        }
        else
        {
            double distSpeedRatio = -diffDist / diffSpeed;
            double d = distSpeedRatio * distSpeedRatio - (diffX * diffX + diffY * diffY) / diffSpeed;
            if (d < 0) return BWAPI::Positions::Invalid;

            double r = sqrt(d);
            t = std::max(distSpeedRatio + r, distSpeedRatio - r);
        }

        if (t < 0) return BWAPI::Positions::Invalid;

        return BWAPI::Position(
                target->getPosition().x + (int) (t * target->getVelocityX()),
                target->getPosition().y + (int) (t * target->getVelocityY()));
    }

    BWAPI::Position CenterOfUnit(BWAPI::TilePosition topLeft, BWAPI::UnitType type)
    {
        return CenterOfUnit(BWAPI::Position(topLeft), type);
    }

    BWAPI::Position CenterOfUnit(BWAPI::Position topLeft, BWAPI::UnitType type)
    {
        // For buildings we assume the top left is the top left of the tile placement, not the top left of the actual building dimensions
        if (type.isBuilding())
        {
            return BWAPI::Position(topLeft.x + type.tileWidth() * 16, topLeft.y + type.tileHeight() * 16);
        }

        return BWAPI::Position(topLeft.x + type.dimensionLeft() + 1, topLeft.y + type.dimensionUp() + 1);
    }

}
