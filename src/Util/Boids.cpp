#include "Boids.h"

#include "Geo.h"
#include "Map.h"
#include "NoGoAreas.h"

#include "DebugFlag_UnitOrders.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DRAW_BOIDS true  // Draws lines for each boid

#if DEBUG_UNIT_BOIDS
#define DEBUG_UNIT_BOIDS_VERBOSE true
#endif
#endif

namespace
{
    bool isValidAndWalkable(const BWAPI::Position &pos)
    {
        return pos.isValid() && Map::isWalkable(pos.x >> 5, pos.y >> 5);
    }

    BWAPI::Position WalkablePositionAlongVector(
            BWAPI::Position start,
            BWAPI::Position vector,
            int minDist)
    {
        int extent = Geo::ApproximateDistance(0, vector.x, 0, vector.y);
        auto furthestWalkable = BWAPI::Positions::Invalid;
        for (int dist = 16; dist < extent; dist += 16)
        {
            auto pos = start + Geo::ScaleVector(vector, dist);
            if (!isValidAndWalkable(pos))
            {
                return furthestWalkable;
            }

            if (dist >= minDist)
            {
                furthestWalkable = pos;
            }
        }

        auto pos = start + vector;
        if (isValidAndWalkable(pos))
        {
            furthestWalkable = pos;
        }

        return furthestWalkable;
    }

    BWAPI::Position WalkablePositionAlongVector(
            BWAPI::Position start,
            BWAPI::Position vector,
            BWAPI::Position &collisionVector)
    {
        int extent = Geo::ApproximateDistance(0, vector.x, 0, vector.y);
        auto furthestWalkable = start;
        collisionVector = Map::collisionVector(start.x >> 5, start.y >> 5);
        for (int dist = 16; dist < extent; dist += 16)
        {
            auto pos = start + Geo::ScaleVector(vector, dist);
            if (!isValidAndWalkable(pos))
            {
                return furthestWalkable;
            }

            furthestWalkable = pos;
            collisionVector += Map::collisionVector(pos.x >> 5, pos.y >> 5);
        }

        auto pos = start + vector;
        if (isValidAndWalkable(pos))
        {
            furthestWalkable = pos;
            collisionVector = BWAPI::Positions::Invalid;
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
        // We prefer tiles that are not in a mineral line
        int closestDist = INT_MAX;
        bool closestIsOutsideMineralLine = false;
        int closestX = unit->lastPosition.x;
        int closestY = unit->lastPosition.y;
        for (int y = unit->tilePositionY - 5; y < unit->tilePositionY + 5; y++)
        {
            if (y < 0 || y >= BWAPI::Broodwar->mapHeight()) continue;

            for (int x = unit->tilePositionX - 5; x < unit->tilePositionX + 5; x++)
            {
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

                if (!unit->isFlying && !Map::isWalkable(x, y)) continue;
                if (NoGoAreas::isNoGo(x, y)) continue;
                if (unit->type.isWorker() && Map::bordersMineralPatch(x, y)) continue;

                auto isInMineralLine = Map::isInOwnMineralLine(x, y);
                if (isInMineralLine && closestIsOutsideMineralLine) continue;

                int dist = Geo::ApproximateDistance(unit->tilePositionX, x, unit->tilePositionY, y);
                if (dist < closestDist || (!isInMineralLine && !closestIsOutsideMineralLine))
                {
                    closestDist = dist;
                    closestX = x * 32 + 16;
                    closestY = y * 32 + 16;
                    closestIsOutsideMineralLine = !isInMineralLine;
                }
            }
        }

        return {closestX, closestY};
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
                                    bool collision)
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

        // If we aren't worried about collisions, just trace a path along the vector and move if there is a walkable position far enough away
        if (!collision)
        {
            auto pos = WalkablePositionAlongVector(unit->lastPosition, vector, minDist);

#if DRAW_BOIDS
            if (pos != BWAPI::Positions::Invalid)
            {
                CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Blue, unit->id);
            }
#endif

            return pos;
        }

        // We want to consider collisions, so trace the path along the vector as usual, but if we meet a collision, try to resolve it
        // We do this by accumulating a collision vector along the path and applying it to the previous position to get out of a collision
        BWAPI::Position collisionVector;
        auto pos = WalkablePositionAlongVector(unit->lastPosition, vector, collisionVector);

        // If there was no collision, use the position
        if (collisionVector == BWAPI::Positions::Invalid)
        {
#if DRAW_BOIDS
            if (pos != BWAPI::Positions::Invalid)
            {
                CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Blue, unit->id);
            }
#endif

            return pos;
        }

        // There was a collision
        // Our approach to resolving collisions is to pull the last found walkable position based on the average collision vector collected
        // during the search
        // We prefer to pull it directly in the direction of the collision vector, but in cases where this is in opposition to our desired
        // movement, we may choose a perpendicular vector instead
        
#if DEBUG_UNIT_BOIDS_VERBOSE
        std::ostringstream dbg;
        dbg << "Collision along vector " << vector << ", walkable pos " << pos;
#endif

        // Scale the vectors so we can apply them incrementally in our collision resolution
        collisionVector = Geo::ScaleVector(collisionVector, 16);
        if (collisionVector == BWAPI::Positions::Invalid)
        {
#if DEBUG_UNIT_BOIDS_VERBOSE
            dbg << "\nNo valid collision vector.";
            CherryVis::log(unit->id) << dbg.str();
#endif
            return BWAPI::Positions::Invalid;
        }
        auto vectorStep = Geo::ScaleVector(vector, 16);

        // Compute perpendicular vectors
        int collisionAngle = Geo::BWDirection(collisionVector);
        int vectorAngle = Geo::BWDirection(vector);
        auto moveTarget = unit->bwapiUnit->getLastCommand().type == BWAPI::UnitCommandTypes::Move
                          ? unit->bwapiUnit->getLastCommand().getTargetPosition()
                          : BWAPI::Positions::Invalid;
        auto getPerpendicularVector = [&](const BWAPI::Position &reference, int &referenceAngle, int &compareAngle)
        {
            auto perpendicularVector = Geo::PerpendicularVector(reference, 16);

            // There are two perpendicular vectors, so we need to pick the best one
            // We use the angle to determine it, unless they are both very close, in which case we use the current move target
            int angleDiff = Geo::BWAngleDiff(compareAngle, Geo::BWAngleAdd(referenceAngle, 64))
                    - Geo::BWAngleDiff(compareAngle, Geo::BWAngleAdd(referenceAngle, -64));
            if (std::abs(angleDiff) < 16)
            {
                int firstDist = moveTarget.getApproxDistance(pos + perpendicularVector);
                int secondDist = moveTarget.getApproxDistance(pos - perpendicularVector);
                if (firstDist < secondDist)
                {
                    return perpendicularVector;
                }
                if (secondDist < firstDist)
                {
                    return BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
                }
            }

            if (angleDiff < 0)
            {
                return perpendicularVector;
            }
            return BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
        };
        auto perpendicularCollisionVector = getPerpendicularVector(collisionVector, collisionAngle, vectorAngle);
        auto perpendicularVector = getPerpendicularVector(vector, vectorAngle, collisionAngle);

        // Main collision adjustment method
#if DEBUG_UNIT_BOIDS_VERBOSE
        auto adjustForCollision = [&pos, &unit, &minDist, &dbg](BWAPI::Position collisionVector, BWAPI::Position vectorStep)
#else
        auto adjustForCollision = [&pos, &unit, &minDist](BWAPI::Position collisionVector, BWAPI::Position vectorStep)
#endif
        {
            auto current = pos;

            auto candidates = {
                    vectorStep,
                    collisionVector,
                    vectorStep + collisionVector,
                    collisionVector + collisionVector,
                    vectorStep + collisionVector + collisionVector
            };

            // Repeatedly add either a step of the vector or the collision vector to get to a walkable tile until we've reached the desired distance
            for (int tries = 0; tries < 6; tries++)
            {
#if DEBUG_UNIT_BOIDS_VERBOSE
                dbg << "\n";
#endif
                for (auto &candidate : candidates)
                {
                    auto next = current + candidate;
#if DEBUG_UNIT_BOIDS_VERBOSE
                    dbg << " " << next;
#endif
                    if (isValidAndWalkable(next))
                    {
#if DEBUG_UNIT_BOIDS_VERBOSE
                        dbg << "*";
#endif
                        current = next;
                        goto validPositionFound;
                    }
                }
#if DEBUG_UNIT_BOIDS_VERBOSE
                dbg << "#";
#endif
                return false;

                validPositionFound:;
                int dist = unit->lastPosition.getApproxDistance(current);
                if (dist >= minDist)
                {
#if DEBUG_UNIT_BOIDS_VERBOSE
                    dbg << "*";
#endif
#if DRAW_BOIDS
                    CherryVis::drawLine(
                            pos.x,
                            pos.y,
                            current.x,
                            current.y,
                            CherryVis::DrawColor::Teal,
                            unit->id);
#endif
                    pos = current;
                    return true;
                }
            }
#if DEBUG_UNIT_BOIDS_VERBOSE
            dbg << "##";
#endif
            // Exceeded max number of tries
            return false;
        };

        // Try in this order
        std::initializer_list<std::pair<BWAPI::Position, BWAPI::Position>> candidates = {
                {collisionVector, vectorStep},
                {collisionVector, perpendicularVector},
                {perpendicularCollisionVector, vectorStep},
                {collisionVector, collisionVector},
        };

        for (const auto &candidate : candidates)
        {
#if DEBUG_UNIT_BOIDS_VERBOSE
            dbg << "\nTrying with collision vector " << candidate.first << " and vector step " << candidate.second;
#endif
            if (adjustForCollision(candidate.first, candidate.second))
            {
#if DEBUG_UNIT_BOIDS_VERBOSE
                CherryVis::log(unit->id) << dbg.str();
#endif

#if DRAW_BOIDS
                CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Blue, unit->id);
#endif
                // If the final position borders an unwalkable tile, move the position to the center of the tile
                // Otherwise we might generate a position where the unit can't actually fit
                if (Map::unwalkableProximity(pos.x >> 5, pos.y >> 5) == 1)
                {
                    pos = {((pos.x >> 5) << 5) + 16, ((pos.y >> 5) << 5) + 16};
                }

                return pos;
            }
        }

#if DEBUG_UNIT_BOIDS_VERBOSE
        CherryVis::log(unit->id) << dbg.str();
#endif
        return BWAPI::Positions::Invalid;
    }
}