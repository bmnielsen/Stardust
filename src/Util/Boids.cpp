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
    bool isValidAndWalkable(const BWAPI::Position &pt)
    {
        return pt.isValid() && Map::isWalkable(pt.x >> 5, pt.y >> 5);
    }

    BWAPI::Position WalkablePositionAlongVector(
            BWAPI::Position start,
            BWAPI::Position vector,
            int minDist)
    {
        int extent = Geom::ApproximateDistance(0, vector.x, 0, vector.y);
        auto furthestWalkable = BWAPI::Positions::Invalid;
        for (int span = 16; span < extent; span += 16)
        {
            auto pt = start + Geom::ScaleVector(vector, span);
            if (!isValidAndWalkable(pt))
            {
                return furthestWalkable;
            }

            if (span >= minDist)
            {
                furthestWalkable = pt;
            }
        }

        auto pt = start + vector;
        if (isValidAndWalkable(pt))
        {
            furthestWalkable = pt;
        }

        return furthestWalkable;
    }

    BWAPI::Position WalkablePositionAlongVector(
            BWAPI::Position start,
            BWAPI::Position vector,
            BWAPI::Position &collisionVector)
    {
        int extent = Geom::ApproximateDistance(0, vector.x, 0, vector.y);
        auto furthestWalkable = start;
        collisionVector = Map::collisionVector(start.x >> 5, start.y >> 5);
        for (int span = 16; span < extent; span += 16)
        {
            auto pt = start + Geom::ScaleVector(vector, span);
            if (!isValidAndWalkable(pt))
            {
                return furthestWalkable;
            }

            furthestWalkable = pt;
            collisionVector += Map::collisionVector(pt.x >> 5, pt.y >> 5);
        }

        auto pt = start + vector;
        if (isValidAndWalkable(pt))
        {
            furthestWalkable = pt;
            collisionVector = BWAPI::Positions::Invalid;
        }

        return furthestWalkable;
    }
}

namespace Boids
{
    BWAPI::Position AvoidNoGoArea(const UnitImpl *gameUnit)
    {
        /* If the unit itself is not currently in a no-go area, just stay put */
        if (!NoGoAreas::isNoGo(gameUnit->tilePositionX, gameUnit->tilePositionY)) return gameUnit->prevPos;

        /* Find the closest tile that is walkable and not in a no-go area */
        /* We prefer tiles that are not in a mineral line */
        int closestDist = INT_MAX;
        bool closestIsOutsideMineralLine = false;
        int closestX = gameUnit->prevPos.x;
        int closestY = gameUnit->prevPos.y;
        for (int y = gameUnit->tilePositionY - 5; y < gameUnit->tilePositionY + 5; y++)
        {
            if (y < 0 || y >= BWAPI::Broodwar->mapHeight()) continue;

            for (int x = gameUnit->tilePositionX - 5; x < gameUnit->tilePositionX + 5; x++)
            {
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

                if (!gameUnit->isFlying && !Map::isWalkable(x, y)) continue;
                if (NoGoAreas::isNoGo(x, y)) continue;
                if (gameUnit->type.isWorker() && Map::bordersMineralPatch(x, y)) continue;

                auto isInMineralLine = Map::isInOwnMineralLine(x, y);
                if (isInMineralLine && closestIsOutsideMineralLine) continue;

                int span = Geom::ApproximateDistance(gameUnit->tilePositionX, x, gameUnit->tilePositionY, y);
                if (span < closestDist || (!isInMineralLine && !closestIsOutsideMineralLine))
                {
                    closestDist = span;
                    closestX = x * 32 + 16;
                    closestY = y * 32 + 16;
                    closestIsOutsideMineralLine = !isInMineralLine;
                }
            }
        }

        return {closestX, closestY};
    }

    void AddSeparation(const UnitImpl *gameUnit, const Unit &other, double detectionLimitFactor, double weight, int &separationX, int &separationY)
    {
        auto span = gameUnit->getDistance(other);
        double detectionLimit = std::max(gameUnit->type.width(), other->type.width()) * detectionLimitFactor;
        if (span >= (int) detectionLimit) return;

        /* We are within the detection limit */
        /* Push away with maximum force at 0 distance, no force at detection limit */
        double distFactor = 1.0 - (double) span / detectionLimit;
        int centerDist = Geom::ApproximateDistance(gameUnit->prevPos.x, other->prevPos.x, gameUnit->prevPos.y, other->prevPos.y);
        if (centerDist == 0) return;
        double scalingFactor = distFactor * distFactor * weight / centerDist;
        separationX -= (int) ((double) (other->prevPos.x - gameUnit->prevPos.x) * scalingFactor);
        separationY -= (int) ((double) (other->prevPos.y - gameUnit->prevPos.y) * scalingFactor);
    }

    void AddSeparation(const UnitImpl *gameUnit, const Unit &other, int detectionLimit, double weight, int &separationX, int &separationY)
    {
        auto span = gameUnit->getDistance(other);
        if (span >= detectionLimit) return;

        /* We are within the detection limit */
        /* Push away with maximum force at 0 distance, no force at detection limit */
        double distFactor = 1.0 - (double) span / (double)detectionLimit;
        int centerDist = Geom::ApproximateDistance(gameUnit->prevPos.x, other->prevPos.x, gameUnit->prevPos.y, other->prevPos.y);
        if (centerDist == 0) return;
        double scalingFactor = distFactor * distFactor * weight / centerDist;
        separationX -= (int) ((double) (other->prevPos.x - gameUnit->prevPos.x) * scalingFactor);
        separationY -= (int) ((double) (other->prevPos.y - gameUnit->prevPos.y) * scalingFactor);
    }

    BWAPI::Position ComputePosition(const UnitImpl *gameUnit,
                                    const std::vector<int> &x,
                                    const std::vector<int> &y,
                                    int scale,
                                    int minDist,
                                    bool collision)
    {
        /* Increase the minimum distance so it is at least the size of the unit */
        minDist = std::max(minDist, std::max(gameUnit->type.width(), gameUnit->type.height()));

        /* The scale must be at least the min dist */
        if (scale > 0) scale = std::max(scale, minDist);

        /* Start by combining into a (possibly scaled) vector */
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

            Telemetry::DrawColor color;
            switch (i)
            {
                case 0:
                    color = Telemetry::DrawColor::Yellow;
                    break;
                case 1:
                    color = Telemetry::DrawColor::Green;
                    break;
                case 2:
                    color = Telemetry::DrawColor::Orange;
                    break;
                default:
                    color = Telemetry::DrawColor::Grey;
                    break;
            }

            Telemetry::drawLine(
                    gameUnit->prevPos.x,
                    gameUnit->prevPos.y,
                    gameUnit->prevPos.x + x[i],
                    gameUnit->prevPos.y + y[i],
                    color,
                    gameUnit->ident);
        }
#endif

        auto vector = (scale > 0)
                      ? Geom::ScaleVector(BWAPI::Position(totalX, totalY), scale)
                      : BWAPI::Position(totalX, totalY);

        /* If the desired target position is inside a no-go area, use special logic instead to avoid the no-go area */
        auto tilePos = BWAPI::TilePosition(gameUnit->prevPos + vector);
        if (tilePos.isValid() && NoGoAreas::isNoGo(tilePos))
        {
            auto pt = AvoidNoGoArea(gameUnit);

#if DRAW_BOIDS
            Telemetry::drawLine(gameUnit->prevPos.x, gameUnit->prevPos.y, pt.x, pt.y, Telemetry::DrawColor::Purple, gameUnit->ident);
#endif

            return pt;
        }

        /* If the vector is zero or invalid, the unit doesn't want to move, so return its current position */
        if (vector == BWAPI::Positions::Invalid || (totalX == 0 && totalY == 0))
        {
            return gameUnit->prevPos;
        }

        /* Flying units don't need to worry about walkability or collisions */
        if (gameUnit->isFlying)
        {
            auto pt = gameUnit->prevPos + vector;

            /* Snap to map edges */
            if (pt.x < 0) pt.x = 0;
            if (pt.x >= BWAPI::Broodwar->mapWidth() * 32) pt.x = (BWAPI::Broodwar->mapWidth() * 32) - 1;
            if (pt.y < 0) pt.y = 0;
            if (pt.y >= BWAPI::Broodwar->mapHeight() * 32) pt.y = (BWAPI::Broodwar->mapHeight() * 32) - 1;

#if DRAW_BOIDS
            if (pt != BWAPI::Positions::Invalid)
            {
                Telemetry::drawLine(gameUnit->prevPos.x, gameUnit->prevPos.y, pt.x, pt.y, Telemetry::DrawColor::Blue, gameUnit->ident);
            }
#endif

            return pt;
        }

        /* If we aren't worried about collisions, just trace a path along the vector and move if there is a walkable position far enough away */
        if (!collision)
        {
            auto pt = WalkablePositionAlongVector(gameUnit->prevPos, vector, minDist);

#if DRAW_BOIDS
            if (pt != BWAPI::Positions::Invalid)
            {
                Telemetry::drawLine(gameUnit->prevPos.x, gameUnit->prevPos.y, pt.x, pt.y, Telemetry::DrawColor::Blue, gameUnit->ident);
            }
#endif

            return pt;
        }

        /* We want to consider collisions, so trace the path along the vector as usual, but if we meet a collision, try to resolve it */
        /* We do this by accumulating a collision vector along the path and applying it to the previous position to get out of a collision */
        BWAPI::Position collisionVector;
        auto pt = WalkablePositionAlongVector(gameUnit->prevPos, vector, collisionVector);

        /* If there was no collision, use the position */
        if (collisionVector == BWAPI::Positions::Invalid)
        {
#if DRAW_BOIDS
            if (pt != BWAPI::Positions::Invalid)
            {
                Telemetry::drawLine(gameUnit->prevPos.x, gameUnit->prevPos.y, pt.x, pt.y, Telemetry::DrawColor::Blue, gameUnit->ident);
            }
#endif

            return pt;
        }

        /* There was a collision */
        /* Our approach to resolving collisions is to pull the last found walkable position based on the average collision vector collected */
        /* during the search */
        /* We prefer to pull it directly in the direction of the collision vector, but in cases where this is in opposition to our desired */
        /* movement, we may choose a perpendicular vector instead */
        
#if DEBUG_UNIT_BOIDS_VERBOSE
        std::ostringstream dbg;
        dbg << "Collision along vector " << vector << ", walkable pos " << pt;
#endif

        /* Scale the vectors so we can apply them incrementally in our collision resolution */
        collisionVector = Geom::ScaleVector(collisionVector, 16);
        if (collisionVector == BWAPI::Positions::Invalid)
        {
#if DEBUG_UNIT_BOIDS_VERBOSE
            dbg << "\nNo valid collision vector.";
            Telemetry::log(gameUnit->ident) << dbg.str();
#endif
            return BWAPI::Positions::Invalid;
        }
        auto vectorStep = Geom::ScaleVector(vector, 16);

        /* Compute perpendicular vectors */
        int collisionAngle = Geom::BWDirection(collisionVector);
        int vectorAngle = Geom::BWDirection(vector);
        auto moveTarget = gameUnit->rawUnit->getLastCommand().type == BWAPI::UnitCommandTypes::Move
                          ? gameUnit->rawUnit->getLastCommand().getTargetPosition()
                          : BWAPI::Positions::Invalid;
        auto getPerpendicularVector = [&](const BWAPI::Position &reference, int &referenceAngle, int &compareAngle)
        {
            auto perpendicularVector = Geom::PerpendicularVector(reference, 16);

            /* There are two perpendicular vectors, so we need to pick the best one */
            /* We use the angle to determine it, unless they are both very close, in which case we use the current move target */
            int angleDiff = Geom::BWAngleDiff(compareAngle, Geom::BWAngleAdd(referenceAngle, 64))
                    - Geom::BWAngleDiff(compareAngle, Geom::BWAngleAdd(referenceAngle, -64));
            if (std::abs(angleDiff) < 16)
            {
                int firstDist = moveTarget.getApproxDistance(pt + perpendicularVector);
                int secondDist = moveTarget.getApproxDistance(pt - perpendicularVector);
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

        /* Main collision adjustment method */
#if DEBUG_UNIT_BOIDS_VERBOSE
        auto adjustForCollision = [&pt, &gameUnit, &minDist, &dbg](BWAPI::Position collisionVector, BWAPI::Position vectorStep)
#else
        auto adjustForCollision = [&pt, &gameUnit, &minDist](BWAPI::Position collisionVector, BWAPI::Position vectorStep)
#endif
        {
            auto current = pt;

            auto candidates = {
                    vectorStep,
                    collisionVector,
                    vectorStep + collisionVector,
                    collisionVector + collisionVector,
                    vectorStep + collisionVector + collisionVector
            };

            /* Repeatedly add either a step of the vector or the collision vector to get to a walkable tile until we've reached the desired distance */
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
                int span = gameUnit->prevPos.getApproxDistance(current);
                if (span >= minDist)
                {
#if DEBUG_UNIT_BOIDS_VERBOSE
                    dbg << "*";
#endif
#if DRAW_BOIDS
                    Telemetry::drawLine(
                            pt.x,
                            pt.y,
                            current.x,
                            current.y,
                            Telemetry::DrawColor::Teal,
                            gameUnit->ident);
#endif
                    pt = current;
                    return true;
                }
            }
#if DEBUG_UNIT_BOIDS_VERBOSE
            dbg << "##";
#endif
            /* Exceeded max number of tries */
            return false;
        };

        /* Try in this order */
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
                Telemetry::log(gameUnit->ident) << dbg.str();
#endif

#if DRAW_BOIDS
                Telemetry::drawLine(gameUnit->prevPos.x, gameUnit->prevPos.y, pt.x, pt.y, Telemetry::DrawColor::Blue, gameUnit->ident);
#endif
                /* If the final position borders an unwalkable tile, move the position to the center of the tile */
                /* Otherwise we might generate a position where the unit can't actually fit */
                if (Map::unwalkableProximity(pt.x >> 5, pt.y >> 5) == 1)
                {
                    pt = {((pt.x >> 5) << 5) + 16, ((pt.y >> 5) << 5) + 16};
                }

                return pt;
            }
        }

#if DEBUG_UNIT_BOIDS_VERBOSE
        Telemetry::log(gameUnit->ident) << dbg.str();
#endif
        return BWAPI::Positions::Invalid;
    }
}