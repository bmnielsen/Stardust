#include "Boids.h"

#include "Geo.h"
#include "Map.h"
#include "NoGoAreas.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DRAW_BOIDS false  // Draws lines for each boid
#endif

namespace
{
    BWAPI::Position WalkablePositionAlongVector(
            BWAPI::Position start,
            BWAPI::Position vector,
            int minDist,
            BWAPI::Position defaultPosition = BWAPI::Positions::Invalid)
    {
        int extent = Geo::ApproximateDistance(0, vector.x, 0, vector.y);
        auto furthestWalkable = defaultPosition;
        for (int dist = 16; dist < extent; dist += 16)
        {
            auto pos = start + Geo::ScaleVector(vector, dist);
            if (!pos.isValid() || !Map::isWalkable(pos.x >> 5, pos.y >> 5))
            {
                return furthestWalkable;
            }

            if (dist >= minDist) furthestWalkable = pos;
        }

        auto pos = start + vector;
        if (pos.isValid() && Map::isWalkable(pos.x >> 5, pos.y >> 5))
        {
            furthestWalkable = pos;
        }

        return furthestWalkable;
    }
}

namespace Boids
{
    BWAPI::Position AvoidNoGoArea(const UnitImpl *unit)
    {
        // If the unit itself is not currently in a no-go area, just stay put
        if (!NoGoAreas::isNoGo(unit->tilePositionX, unit->tilePositionY)) return unit->lastPosition;

        // Find the closest tile that is walkable and not in a no-go area
        int closestDist = INT_MAX;
        int closestX = unit->lastPosition.x;
        int closestY = unit->lastPosition.y;
        for (int x = unit->tilePositionX - 5; x < unit->tilePositionX + 5; x++)
        {
            if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

            for (int y = unit->tilePositionY - 5; y < unit->tilePositionY + 5; y++)
            {
                if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;
                if (!Map::isWalkable(x, y)) continue;
                if (NoGoAreas::isNoGo(x, y)) continue;

                int dist = Geo::ApproximateDistance(unit->tilePositionX, x, unit->tilePositionY, y);
                if (dist < closestDist)
                {
                    closestDist = dist;
                    closestX = x * 32 + 16;
                    closestY = y * 32 + 16;
                }
            }
        }

        return BWAPI::Position(closestX, closestY);
    }

    void AddSeparation(const UnitImpl *unit, const Unit &other, double detectionLimitFactor, double weight, int &separationX, int &separationY)
    {
        auto dist = unit->getDistance(other);
        double detectionLimit = std::max(unit->type.width(), other->type.width()) * detectionLimitFactor;
        if (dist >= (int) detectionLimit) return;

        // We are within the detection limit
        // Push away with maximum force at 0 distance, no force at detection limit
        double distFactor = 1.0 - (double) dist / detectionLimit;
        int centerDist = Geo::ApproximateDistance(unit->lastPosition.x, other->lastPosition.x, unit->lastPosition.y, other->lastPosition.y);
        if (centerDist == 0) return;
        double scalingFactor = distFactor * distFactor * weight / centerDist;
        separationX -= (int) ((double) (other->lastPosition.x - unit->lastPosition.x) * scalingFactor);
        separationY -= (int) ((double) (other->lastPosition.y - unit->lastPosition.y) * scalingFactor);
    }

    BWAPI::Position ComputePosition(const UnitImpl *unit,
                                    const std::vector<int> &x,
                                    const std::vector<int> &y,
                                    int scale,
                                    int minDist,
                                    int collisionWeight)
    {
        // Increase the minimum distance so it is at least the size of the unit
        minDist = std::max(minDist, std::max(unit->type.width(), unit->type.height()));

        // Start by combining into a (possibly scaled) vector
        int totalX = 0;
        for (const auto &xval : x)
        {
            totalX += xval;
        }

        int totalY = 0;
        for (const auto &yval : y)
        {
            totalY += yval;
        }

#if DRAW_BOIDS
        for (int i = 0; i < x.size() && i < y.size(); i++)
        {
            if (x[i] == 0 && y[i] == 0) continue;

            CherryVis::DrawColor color;
            switch (i)
            {
                case 0:
                    color = CherryVis::DrawColor::Yellow;
                    break;
                case 1:
                    color = CherryVis::DrawColor::Green;
                    break;
                case 2:
                    color = CherryVis::DrawColor::Orange;
                    break;
                default:
                    color = CherryVis::DrawColor::Grey;
                    break;
            }

            CherryVis::drawLine(
                    unit->lastPosition.x,
                    unit->lastPosition.y,
                    unit->lastPosition.x + x[i],
                    unit->lastPosition.y + y[i],
                    color);
        }
#endif

        auto vector = (scale > 0)
                      ? Geo::ScaleVector(BWAPI::Position(totalX, totalY), scale)
                      : BWAPI::Position(totalX, totalY);

        // If the desired target position is inside a no-go area, use special logic instead to avoid the no-go area
        auto tile = BWAPI::TilePosition(unit->lastPosition + vector);
        if (tile.isValid() && NoGoAreas::isNoGo(tile))
        {
            auto pos = AvoidNoGoArea(unit);

#if DRAW_BOIDS
            CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Purple);
#endif

            return pos;
        }

        // If the vector is zero or invalid, the unit doesn't want to move, so return its current position
        if (vector == BWAPI::Positions::Invalid || (totalX == 0 && totalY == 0))
        {
            return unit->lastPosition;
        }

        // Get the furthest walkable position along the vector
        // This returns invalid if the unit cannot move in this direction
        auto pos = WalkablePositionAlongVector(unit->lastPosition, vector, minDist);

        // Consider collision if specified
        if (collisionWeight > 0)
        {
            auto collisionReferencePos = (pos == BWAPI::Positions::Invalid) ? unit->lastPosition : pos;

            auto collisionVector = Map::collisionVector(collisionReferencePos.x >> 5, collisionReferencePos.y >> 5) * collisionWeight;
            if (collisionVector.x != 0 || collisionVector.y != 0)
            {
#if DRAW_BOIDS
                CherryVis::drawLine(
                        unit->lastPosition.x,
                        unit->lastPosition.y,
                        collisionReferencePos.x + collisionVector.x,
                        collisionReferencePos.y + collisionVector.y,
                        CherryVis::DrawColor::Teal);
#endif

                auto totalVector = (collisionReferencePos + collisionVector) - unit->lastPosition;

                // In the case of collisions, we always make sure to scale to at least 64
                auto magnitude = Geo::ApproximateDistance(totalVector.x, 0, totalVector.y, 0);
                if (magnitude < 64)
                {
                    totalVector = Geo::ScaleVector(totalVector, 64);
                }

                if (totalVector != BWAPI::Positions::Invalid)
                {
                    pos = WalkablePositionAlongVector(unit->lastPosition, totalVector, minDist, pos);
                }
            }
        }

#if DRAW_BOIDS
        if (pos != BWAPI::Positions::Invalid)
        {
            CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Blue);
        }
#endif

        return pos;
    }
}