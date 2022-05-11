#include "Boids.h"

#include "Geo.h"
#include "Map.h"
#include "NoGoAreas.h"

#include "DebugFlag_UnitOrders.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DRAW_BOIDS true  // Draws lines for each boid
#endif

namespace
{
    void CheckCollision(const BWAPI::Position &pos, BWAPI::Position *collisionStart, BWAPI::Position *collisionVector)
    {
        if (!collisionStart || *collisionStart != BWAPI::Positions::Invalid) return;

        auto collisionHere = Map::collisionVector(pos.x >> 5, pos.y >> 5);
        if (collisionHere.x != 0 || collisionHere.y != 0)
        {
            *collisionStart = pos;
            *collisionVector = collisionHere;
        }
    }

    BWAPI::Position WalkablePositionAlongVector(
            BWAPI::Position start,
            BWAPI::Position vector,
            int minDist,
            BWAPI::Position defaultPosition,
            BWAPI::Position *collisionStart = nullptr,
            BWAPI::Position *collisionVector = nullptr)
    {
        CheckCollision(start, collisionStart, collisionVector);

        int extent = Geo::ApproximateDistance(0, vector.x, 0, vector.y);
        auto furthestWalkable = defaultPosition;
        for (int dist = 16; dist < extent; dist += 16)
        {
            auto pos = start + Geo::ScaleVector(vector, dist);
            if (!pos.isValid() || !Map::isWalkable(pos.x >> 5, pos.y >> 5))
            {
                return furthestWalkable;
            }

            CheckCollision(pos, collisionStart, collisionVector);

            if (dist >= minDist)
            {
                furthestWalkable = pos;
            }
        }

        auto pos = start + vector;
        if (pos.isValid() && Map::isWalkable(pos.x >> 5, pos.y >> 5))
        {
            CheckCollision(pos, collisionStart, collisionVector);

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
        for (int y = unit->tilePositionY - 5; y < unit->tilePositionY + 5; y++)
        {
            if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

            for (int x = unit->tilePositionX - 5; x < unit->tilePositionX + 5; x++)
            {
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

                if (!unit->isFlying && !Map::isWalkable(x, y)) continue;
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

    void AddSeparation(const UnitImpl *unit, const Unit &other, int detectionLimit, double weight, int &separationX, int &separationY)
    {
        auto dist = unit->getDistance(other);
        if (dist >= detectionLimit) return;

        // We are within the detection limit
        // Push away with maximum force at 0 distance, no force at detection limit
        double distFactor = 1.0 - (double) dist / (double)detectionLimit;
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

        // The scale must be at least the min dist
        if (scale > 0) scale = std::max(scale, minDist);

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
                    color,
                    unit->id);
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
            CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Purple, unit->id);
#endif

            return pos;
        }

        // If the vector is zero or invalid, the unit doesn't want to move, so return its current position
        if (vector == BWAPI::Positions::Invalid || (totalX == 0 && totalY == 0))
        {
            return unit->lastPosition;
        }

        // Flying units don't need to worry about walkability or collisions
        if (unit->isFlying)
        {
            auto pos = unit->lastPosition + vector;

            // Snap to map edges
            if (pos.x < 0) pos.x = 0;
            if (pos.x >= BWAPI::Broodwar->mapWidth() * 32) pos.x = (BWAPI::Broodwar->mapWidth() * 32) - 1;
            if (pos.y < 0) pos.y = 0;
            if (pos.y >= BWAPI::Broodwar->mapHeight() * 32) pos.y = (BWAPI::Broodwar->mapHeight() * 32) - 1;

#if DRAW_BOIDS
            if (pos != BWAPI::Positions::Invalid)
            {
                CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Blue, unit->id);
            }
#endif

            return pos;
        }

        BWAPI::Position pos;

        // Consider collision if specified
        if (collisionWeight > 0)
        {
            // Get the furthest walkable position along the vector, tracking the first position with a collision vector
            // This returns invalid if the unit cannot move in this direction
            BWAPI::Position collisionStart = BWAPI::Positions::Invalid;
            BWAPI::Position collisionVector = BWAPI::Positions::Invalid;
            pos = WalkablePositionAlongVector(unit->lastPosition, vector, minDist, BWAPI::Positions::Invalid, &collisionStart, &collisionVector);
            if (collisionVector != BWAPI::Positions::Invalid)
            {
                // Check if the collision vector is in the opposite direction of our desired target vector
                // If this is the case, move perpendicularly to it instead
                int desiredAngle = Geo::BWDirection(vector);
                int collisionAngle = Geo::BWDirection(collisionVector);
                if (Geo::BWAngleDiff(desiredAngle, collisionAngle) > 100)
                {
                    // Generate the perpendicular points
                    auto perpendicularVector = Geo::PerpendicularVector(
                            collisionVector, Geo::ApproximateDistance(0, collisionVector.x, 0, collisionVector.y));
                    auto firstPos = collisionStart + perpendicularVector;
                    auto secondPos = collisionStart - perpendicularVector;

                    // Pick the point that is in the direction of our current motion
                    auto futurePos = unit->bwapiUnit->getLastCommand().getTargetPosition();
                    if (!futurePos.isValid()) futurePos = unit->predictPosition(3);
                    int firstDist = firstPos.getApproxDistance(futurePos);
                    int secondDist = secondPos.getApproxDistance(futurePos);
                    if (firstDist <= secondDist)
                    {
                        collisionVector = perpendicularVector;
                    }
                    else
                    {
                        collisionVector = BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
                    }

                    CherryVis::log(unit->id) << "Perpendicular collision: "
                                             << "; futurePos pos " << BWAPI::WalkPosition(futurePos)
                                             << "; first pos: " << firstDist << "@" << BWAPI::WalkPosition(firstPos)
                                             << "; second pos: " << secondDist << "@" << BWAPI::WalkPosition(secondPos);
                }

                // If we couldn't find a walkable location along the vector, move towards the collision vector directly
                if (pos == BWAPI::Positions::Invalid)
                {
#if DRAW_BOIDS
                    CherryVis::drawLine(
                            collisionStart.x,
                            collisionStart.y,
                            collisionStart.x + collisionVector.x * collisionWeight,
                            collisionStart.y + collisionVector.y * collisionWeight,
                            CherryVis::DrawColor::Teal,
                            unit->id);
#endif

                    // Compute the vector from the current position to the result of adding the collision vector to the collision start position
                    auto totalVector = (collisionStart + collisionVector * collisionWeight) - unit->lastPosition;

                    // Scale it and find a walkable position along it
                    totalVector = Geo::ScaleVector(totalVector, std::min(64, scale));
                    if (totalVector != BWAPI::Positions::Invalid)
                    {
                        pos = WalkablePositionAlongVector(unit->lastPosition, totalVector, minDist, collisionStart + collisionVector);
                    }

#if DEBUG_UNIT_BOIDS
                    CherryVis::log(unit->id) << "Collision start: " << BWAPI::WalkPosition(collisionStart)
                                             << "; collisionVector: " << BWAPI::WalkPosition(collisionStart + collisionVector * collisionWeight)
                                             << "; updated pos: " << BWAPI::WalkPosition(pos);
#endif
                }
                else
                {
                    // We could find a walkable position along our vector
                    // But if the position we found is closer than the desired one, apply a bit of force away from the collision
                    int posDist = Geo::ApproximateDistance(pos.x, unit->lastPosition.x, pos.y, unit->lastPosition.y);
                    int vectorLength = Geo::ApproximateDistance(vector.x, 0, vector.y, 0);
                    float weight = (float)collisionWeight * (1.0f - (float)posDist / (float)vectorLength);
                    collisionVector = {(int)((float)collisionVector.x * weight), (int)((float)collisionVector.y * weight)};

#if DEBUG_UNIT_BOIDS
                    CherryVis::log(unit->id) << "Collision start: " << BWAPI::WalkPosition(collisionStart)
                                             << "; collisionVector: " << BWAPI::WalkPosition(collisionStart + collisionVector)
                                             << "; weight: " << weight
                                             << "; updated pos: " << BWAPI::WalkPosition(pos + collisionVector);
#endif

                    auto updatedPos = pos + collisionVector;
                    if (updatedPos.isValid() && Map::isWalkable(updatedPos.x >> 5, updatedPos.y >> 5))
                    {
#if DRAW_BOIDS
                        CherryVis::drawLine(
                                pos.x,
                                pos.y,
                                pos.x + collisionVector.x,
                                pos.y + collisionVector.y,
                                CherryVis::DrawColor::Teal,
                                unit->id);
#endif
                        pos = updatedPos;
                    }
                }
            }
        }
        else
        {
            // Get the furthest walkable position along the vector
            // This returns invalid if the unit cannot move in this direction
            pos = WalkablePositionAlongVector(unit->lastPosition, vector, minDist, BWAPI::Positions::Invalid);
        }

#if DRAW_BOIDS
        if (pos != BWAPI::Positions::Invalid)
        {
            CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Blue, unit->id);
        }
#endif

        return pos;
    }
}