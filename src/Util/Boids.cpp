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

#if DEBUG_UNIT_BOIDS
        std::ostringstream dbg;
        dbg << "Collision along vector " << vector << ", walkable pos " << pos;
#endif

        // Scale the vectors so we can apply them incrementally in our collision resolution
        collisionVector = Geo::ScaleVector(collisionVector, 16);
        if (collisionVector == BWAPI::Positions::Invalid)
        {
#if DEBUG_UNIT_BOIDS
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
#if DEBUG_UNIT_BOIDS
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
#if DEBUG_UNIT_BOIDS
                dbg << "\n";
#endif
                for (auto &candidate : candidates)
                {
                    auto next = current + candidate;
#if DEBUG_UNIT_BOIDS
                    dbg << " " << next;
#endif
                    if (isValidAndWalkable(next))
                    {
#if DEBUG_UNIT_BOIDS
                        dbg << "*";
#endif
                        current = next;
                        goto validPositionFound;
                    }
                }
#if DEBUG_UNIT_BOIDS
                dbg << "#";
#endif
                return false;

                validPositionFound:;
                int dist = unit->lastPosition.getApproxDistance(current);
                if (dist >= minDist)
                {
#if DEBUG_UNIT_BOIDS
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
#if DEBUG_UNIT_BOIDS
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
#if DEBUG_UNIT_BOIDS
            dbg << "\nTrying with collision vector " << candidate.first << " and vector step " << candidate.second;
#endif
            if (adjustForCollision(candidate.first, candidate.second))
            {
#if DEBUG_UNIT_BOIDS
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

#if DEBUG_UNIT_BOIDS
        CherryVis::log(unit->id) << dbg.str();
#endif
        return BWAPI::Positions::Invalid;
        /*

        // First try to adjust the collision using the vector as-is
        if (!adjustForCollision())
        {
            auto getPerpendicularVector = [](BWAPI::Position reference, int referenceAngle, int compareAngle)
            {
                auto perpendicularVector = Geo::PerpendicularVector(reference, 16);

                if (Geo::BWAngleDiff(compareAngle, Geo::BWAngleAdd(referenceAngle, 64))
                    < Geo::BWAngleDiff(compareAngle, Geo::BWAngleAdd(referenceAngle, -64)))
                {
                    return perpendicularVector;
                }
                return BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
            };

            // That didn't work. Try using a vector perpendicular to the collision vector instead.
            int vectorAngle = Geo::BWDirection(vector);
            int collisionAngle = Geo::BWDirection(collisionVector);
            collisionVector = getPerpendicularVector(collisionVector, collisionAngle, vectorAngle);
#if DEBUG_UNIT_BOIDS
            dbg << "\nTrying with perpendicular collision vector " << collisionVector
                << "; vectorAngle=" << vectorAngle
                << "; collisionAngle=" << collisionAngle;
#endif
            if (!adjustForCollision())
            {
                // That didn't work either. Try a perpendicular vector to our target vector.
                collisionVector = getPerpendicularVector(vector, vectorAngle, collisionAngle);
#if DEBUG_UNIT_BOIDS
                dbg << "\nTrying with perpendicular target vector " << collisionVector
                    << "; vectorAngle=" << vectorAngle
                    << "; collisionAngle=" << collisionAngle;
#endif

                if (!adjustForCollision())
                {
                    // That didn't work either, so we're probably in a tight spot
                    // Just use the collision vector from the current location instead
                    collisionVector = Geo::ScaleVector(
                            Map::collisionVector(unit->lastPosition.x >> 5, unit->lastPosition.y >> 5),
                            std::max(scale, minDist));
#if DEBUG_UNIT_BOIDS
                    dbg << "\nTrying with collision vector at unit's position " << collisionVector;
#endif
                    if (collisionVector == BWAPI::Positions::Invalid)
                    {
                        pos = BWAPI::Positions::Invalid;
                    }
                    else
                    {
#if DRAW_BOIDS
                        CherryVis::drawLine(
                                unit->lastPosition.x,
                                unit->lastPosition.y,
                                unit->lastPosition.x + collisionVector.x,
                                unit->lastPosition.y + collisionVector.y,
                                CherryVis::DrawColor::Teal,
                                unit->id);
#endif

                        pos = WalkablePositionAlongVector(unit->lastPosition, collisionVector, 0);
                    }
                }
            }
        }

#if DEBUG_UNIT_BOIDS
        CherryVis::log(unit->id) << dbg.str();
#endif

#if DRAW_BOIDS
        if (pos != BWAPI::Positions::Invalid)
        {
            CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, pos.x, pos.y, CherryVis::DrawColor::Blue, unit->id);
        }
#endif

        return pos;

        int desiredDist = Geo::ApproximateDistance(0, vector.x, 0, vector.y);
        auto current = unit->lastPosition;
        auto step = Geo::ScaleVector(vector, 16);
        auto furthestWalkable = BWAPI::Positions::Invalid;
        BWAPI::Position collisionVector = BWAPI::Positions::Invalid;

        auto updateCollisionVector = [&](bool force = false)
        {
            if (collisionVector != BWAPI::Positions::Invalid && !force) return;

            // Look up the vector, which we expect to exist, as the current position is always walkable-bordering-unwalkable
            collisionVector = Map::collisionVector(current.x >> 5, current.y >> 5);
            if (collisionVector.x == 0 && collisionVector.y == 0)
            {
#if DEBUG_UNIT_BOIDS
                dbg << "no collision vector!";
#endif
                collisionVector = BWAPI::Positions::Invalid;
                return;
            }

            // If the collision vector is pointing in the opposite direction of our desired target vector,
            // generate a perpendicular vector instead
            auto vectorAngle = Geo::BWDirection(vector);
            int collisionAngle = Geo::BWDirection(collisionVector);
            if (Geo::BWAngleDiff(vectorAngle, collisionAngle) > 100)
            {
                // Generate the perpendicular points
                auto perpendicularVector = Geo::PerpendicularVector(collisionVector, 32);
                auto firstPos = current + perpendicularVector;
                auto secondPos = current - perpendicularVector;

#if DEBUG_UNIT_BOIDS
                dbg << "perpendicular " << perpendicularVector << "; considering " << firstPos << "," << secondPos << ": ";
#endif

                // Pick the best one, starting by considering walkability
                if (!isValidAndWalkable(firstPos))
                {
                    if (!isValidAndWalkable(secondPos))
                    {
#if DEBUG_UNIT_BOIDS
                        dbg << "no valid perpendicular collision vector";
#endif
                        collisionVector = BWAPI::Positions::Invalid;
                        return;
                    }

                    collisionVector = BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
                    return;
                }

                if (!isValidAndWalkable(secondPos))
                {
                    collisionVector = perpendicularVector;
                    return;
                }

                // Neither is walkable, so consider which is closest to our current motion
                auto movingTowards = unit->bwapiUnit->getLastCommand().getTargetPosition();
                if (!movingTowards.isValid()) movingTowards = unit->predictPosition(3);
                int firstDist = firstPos.getApproxDistance(movingTowards);
                int secondDist = secondPos.getApproxDistance(movingTowards);
                if (firstDist <= secondDist)
                {
                    collisionVector = perpendicularVector;
                }
                else
                {
                    collisionVector = BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
                }
            }
            else
            {
                collisionVector = Geo::ScaleVector(collisionVector, 16);
            }
        };

#if DEBUG_UNIT_BOIDS
        std::ostringstream dbg;
        dbg << "Tracing from " << current << " along " << vector << " of " << desiredDist;
#endif

        for (int dist = 16; dist < (desiredDist + 16); dist += 16)
        {
#if DEBUG_UNIT_BOIDS
            dbg << "\n" << dist << ": " << current << ": ";
#endif

            auto next = current + step;
            if (isValidAndWalkable(next))
            {
#if DEBUG_UNIT_BOIDS
                dbg << "to " << next;
#endif
            }
            else
            {
                // There is a collision
#if DEBUG_UNIT_BOIDS
                dbg << "collision: ";
#endif

                auto computeCollisionVector = [&]()
                {
                    // Look up the vector, which we expect to exist, as the current position is always walkable-bordering-unwalkable
                    collisionVector = Map::collisionVector(current.x >> 5, current.y >> 5);
                    if (collisionVector.x == 0 && collisionVector.y == 0)
                    {
#if DEBUG_UNIT_BOIDS
                        dbg << "no collision vector!";
#endif
                        collisionVector = BWAPI::Positions::Invalid;
                        return;
                    }

                    // If the collision vector is pointing in the opposite direction of our desired target vector,
                    // generate a perpendicular vector instead
                    auto vectorAngle = Geo::BWDirection(vector);
                    int collisionAngle = Geo::BWDirection(collisionVector);
                    if (Geo::BWAngleDiff(vectorAngle, collisionAngle) > 100)
                    {
                        // Generate the perpendicular points
                        auto perpendicularVector = Geo::PerpendicularVector(collisionVector, 32);
                        auto firstPos = current + perpendicularVector;
                        auto secondPos = current - perpendicularVector;

#if DEBUG_UNIT_BOIDS
                        dbg << "perpendicular " << perpendicularVector << "; considering " << firstPos << "," << secondPos << ": ";
#endif

                        // Pick the best one, starting by considering walkability
                        if (!isValidAndWalkable(firstPos))
                        {
                            if (!isValidAndWalkable(secondPos))
                            {
#if DEBUG_UNIT_BOIDS
                                dbg << "no valid perpendicular collision vector";
#endif
                                collisionVector = BWAPI::Positions::Invalid;
                                return;
                            }

                            collisionVector = BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
                            return;
                        }

                        if (!isValidAndWalkable(secondPos))
                        {
                            collisionVector = perpendicularVector;
                            return;
                        }

                        // Neither is walkable, so consider which is closest to our current motion
                        auto movingTowards = unit->bwapiUnit->getLastCommand().getTargetPosition();
                        if (!movingTowards.isValid()) movingTowards = unit->predictPosition(3);
                        int firstDist = firstPos.getApproxDistance(movingTowards);
                        int secondDist = secondPos.getApproxDistance(movingTowards);
                        if (firstDist <= secondDist)
                        {
                            collisionVector = perpendicularVector;
                        }
                        else
                        {
                            collisionVector = BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
                        }
                    }
                    else
                    {
                        collisionVector = Geo::ScaleVector(collisionVector, 16);
                    }
                };

                if (collisionVector == BWAPI::Positions::Invalid)
                {
                    computeCollisionVector();
                    if (collisionVector == BWAPI::Positions::Invalid)
                    {
                        break;
                    }

                    next = current + collisionVector;
#if DEBUG_UNIT_BOIDS
                    dbg << "vector: " << collisionVector;
#endif
                }
                else
                {
                    next = current + collisionVector;
                    if (!isValidAndWalkable(next))
                    {
                        computeCollisionVector();
                        if (collisionVector == BWAPI::Positions::Invalid)
                        {
                            break;
                        }

                        next = current + collisionVector;

#if DEBUG_UNIT_BOIDS
                        dbg << "vector: " << collisionVector << " (recomputed)";
#endif
                    }
                    else
                    {
#if DEBUG_UNIT_BOIDS
                        dbg << "vector: " << collisionVector << " (reused)";
#endif
                    }
                }

#if DEBUG_UNIT_BOIDS
                dbg << " to " << next;
#endif

                if (!isValidAndWalkable(next))
                {
#if DEBUG_UNIT_BOIDS
                    dbg << " (UNWALKABLE)";
#endif
                    break;
                }

#if DRAW_BOIDS
                CherryVis::drawLine(
                        current.x,
                        current.y,
                        next.x,
                        next.y,
                        CherryVis::DrawColor::Teal,
                        unit->id);
#endif
            }

            current = next;
            if (dist >= minDist)
            {
                furthestWalkable = current;
#if DEBUG_UNIT_BOIDS
                dbg << "*";
#endif
            }
        }

#if DEBUG_UNIT_BOIDS
        CherryVis::log(unit->id) << dbg.str();
#endif

//
//                if (collisionVector == BWAPI::Positions::Invalid) computeCollisionVector();
//                if (collisionVector == BWAPI::Positions::Invalid) break;
//
//
//#if DEBUG_UNIT_BOIDS
//                if (!collisionDebug)
//                {
//                    collisionDebug = true;
//                }
//                else
//                {
//                    dbg << "\n";
//                }
//                dbg << BWAPI::WalkPosition(current) << ": ";
//#endif
//                // Look up the collision vector
//                // Abort if we can't find it, but we should always have one
//                auto collisionVector = Map::collisionVector(current.x >> 5, current.y >> 5);
//                if (collisionVector.x == 0 && collisionVector.y == 0)
//                {
//#if DEBUG_UNIT_BOIDS
//                    dbg << "No collision vector!";
//#endif
//                    break;
//                }
//
//                // If the collision vector is pointing in the opposite direction of our desired target vector,
//                // generate a perpendicular vector instead
//                if (vectorAngle == INT_MAX) vectorAngle = Geo::BWDirection(vector);
//                int collisionAngle = Geo::BWDirection(collisionVector);
//                if (Geo::BWAngleDiff(vectorAngle, collisionAngle) > 100)
//                {
//                    // Generate the perpendicular points
//                    auto perpendicularVector = Geo::PerpendicularVector(vector, 16);
//                    auto firstPos = current + perpendicularVector;
//                    auto secondPos = current - perpendicularVector;
//
//                    // Pick the best one, starting by considering walkability
//                    if (!isValidAndWalkable(firstPos))
//                    {
//                        if (!isValidAndWalkable(secondPos))
//                        {
//#if DEBUG_UNIT_BOIDS
//                            dbg << "no valid perpendicular collision vector";
//#endif
//                            break;
//                        }
//                        next = secondPos;
//                    }
//                    else if (!isValidAndWalkable(secondPos))
//                    {
//                        next = firstPos;
//                    }
//                    else
//                    {
//                        // Neither is walkable, so consider which is closest to our current motion
//                        if (movingTowards == BWAPI::Positions::Invalid)
//                        {
//                            movingTowards = unit->bwapiUnit->getLastCommand().getTargetPosition();
//                            if (!movingTowards.isValid()) movingTowards = unit->predictPosition(3);
//                        }
//                        int firstDist = firstPos.getApproxDistance(movingTowards);
//                        int secondDist = secondPos.getApproxDistance(movingTowards);
//                        if (firstDist <= secondDist)
//                        {
//                            next = firstPos;
//                        }
//                        else
//                        {
//                            next = secondPos;
//                        }
//                    }
//
//#if DEBUG_UNIT_BOIDS
//                    dbg << "collision vector: " << BWAPI::WalkPosition(next) << "*";
//#endif
//                }
//                else
//                {
//                    next = current + Geo::ScaleVector(collisionVector, 16);
//#if DEBUG_UNIT_BOIDS
//                    dbg << "collision vector: " << BWAPI::WalkPosition(next) << "*";
//#endif
//                    if (!isValidAndWalkable(next))
//                    {
//#if DEBUG_UNIT_BOIDS
//                        dbg << " - UNWALKABLE";
//#endif
//                        break;
//                    }
//                }
//
//#if DRAW_BOIDS
//                CherryVis::drawLine(
//                        current.x,
//                        current.y,
//                        next.x,
//                        next.y,
//                        CherryVis::DrawColor::Teal,
//                        unit->id);
//#endif
//            }


//
//        BWAPI::Position pos;
//
//        // Consider collision if specified
//        if (collisionWeight > 0)
//        {
//            // Get the furthest walkable position along the vector, tracking the first position with a collision vector
//            // This returns invalid if the unit cannot move in this direction
//            BWAPI::Position collisionStart = BWAPI::Positions::Invalid;
//            BWAPI::Position collisionVector = BWAPI::Positions::Invalid;
//            pos = WalkablePositionAlongVector(unit->lastPosition, vector, minDist, BWAPI::Positions::Invalid, &collisionStart, &collisionVector);
//            if (collisionVector != BWAPI::Positions::Invalid)
//            {
//                // Check if the collision vector is in the opposite direction of our desired target vector
//                // If this is the case, move perpendicularly to it instead
//                int desiredAngle = Geo::BWDirection(vector);
//                int collisionAngle = Geo::BWDirection(collisionVector);
//                if (Geo::BWAngleDiff(desiredAngle, collisionAngle) > 100)
//                {
//                    // Generate the perpendicular points
//                    auto perpendicularVector = Geo::PerpendicularVector(
//                            collisionVector, Geo::ApproximateDistance(0, collisionVector.x, 0, collisionVector.y));
//                    auto firstPos = collisionStart + perpendicularVector;
//                    auto secondPos = collisionStart - perpendicularVector;
//
//                    // Pick the point that is in the direction of our current motion
//                    auto futurePos = unit->bwapiUnit->getLastCommand().getTargetPosition();
//                    if (!futurePos.isValid()) futurePos = unit->predictPosition(3);
//                    int firstDist = firstPos.getApproxDistance(futurePos);
//                    int secondDist = secondPos.getApproxDistance(futurePos);
//                    if (firstDist <= secondDist)
//                    {
//                        collisionVector = perpendicularVector;
//                    }
//                    else
//                    {
//                        collisionVector = BWAPI::Position(-perpendicularVector.x, -perpendicularVector.y);
//                    }
//
//                    CherryVis::log(unit->id) << "Perpendicular collision: "
//                                             << "; futurePos pos " << BWAPI::WalkPosition(futurePos)
//                                             << "; first pos: " << firstDist << "@" << BWAPI::WalkPosition(firstPos)
//                                             << "; second pos: " << secondDist << "@" << BWAPI::WalkPosition(secondPos);
//                }
//
//                // If we couldn't find a walkable location along the vector, move towards the collision vector directly
//                if (pos == BWAPI::Positions::Invalid)
//                {
//#if DRAW_BOIDS
//                    CherryVis::drawLine(
//                            collisionStart.x,
//                            collisionStart.y,
//                            collisionStart.x + collisionVector.x * collisionWeight,
//                            collisionStart.y + collisionVector.y * collisionWeight,
//                            CherryVis::DrawColor::Teal,
//                            unit->id);
//#endif
//
//                    // Compute the vector from the current position to the result of adding the collision vector to the collision start position
//                    auto totalVector = (collisionStart + collisionVector * collisionWeight) - unit->lastPosition;
//
//                    // Scale it and find a walkable position along it
//                    totalVector = Geo::ScaleVector(totalVector, std::min(64, scale));
//                    if (totalVector != BWAPI::Positions::Invalid)
//                    {
//                        pos = WalkablePositionAlongVector(unit->lastPosition, totalVector, minDist, collisionStart + collisionVector);
//                    }
//
//#if DEBUG_UNIT_BOIDS
//                    CherryVis::log(unit->id) << "Collision start: " << BWAPI::WalkPosition(collisionStart)
//                                             << "; collisionVector: " << BWAPI::WalkPosition(collisionStart + collisionVector * collisionWeight)
//                                             << "; updated pos: " << BWAPI::WalkPosition(pos);
//#endif
//                }
//                else
//                {
//                    // We could find a walkable position along our vector
//                    // But if the position we found is closer than the desired one, apply a bit of force away from the collision
//                    int posDist = Geo::ApproximateDistance(pos.x, unit->lastPosition.x, pos.y, unit->lastPosition.y);
//                    int vectorLength = Geo::ApproximateDistance(vector.x, 0, vector.y, 0);
//                    float weight = (float)collisionWeight * (1.0f - (float)posDist / (float)vectorLength);
//                    collisionVector = {(int)((float)collisionVector.x * weight), (int)((float)collisionVector.y * weight)};
//
//#if DEBUG_UNIT_BOIDS
//                    CherryVis::log(unit->id) << "Collision start: " << BWAPI::WalkPosition(collisionStart)
//                                             << "; collisionVector: " << BWAPI::WalkPosition(collisionStart + collisionVector)
//                                             << "; weight: " << weight
//                                             << "; updated pos: " << BWAPI::WalkPosition(pos + collisionVector);
//#endif
//
//                    auto updatedPos = pos + collisionVector;
//                    if (updatedPos.isValid() && Map::isWalkable(updatedPos.x >> 5, updatedPos.y >> 5))
//                    {
//#if DRAW_BOIDS
//                        CherryVis::drawLine(
//                                pos.x,
//                                pos.y,
//                                pos.x + collisionVector.x,
//                                pos.y + collisionVector.y,
//                                CherryVis::DrawColor::Teal,
//                                unit->id);
//#endif
//                        pos = updatedPos;
//                    }
//                }
//            }
//        }
//        else
//        {
//            // Get the furthest walkable position along the vector
//            // This returns invalid if the unit cannot move in this direction
//            pos = WalkablePositionAlongVector(unit->lastPosition, vector, minDist, BWAPI::Positions::Invalid);
//        }

#if DRAW_BOIDS
        if (furthestWalkable != BWAPI::Positions::Invalid)
        {
            CherryVis::drawLine(unit->lastPosition.x, unit->lastPosition.y, furthestWalkable.x, furthestWalkable.y, CherryVis::DrawColor::Blue, unit->id);
        }
#endif

        return furthestWalkable;
         */
    }
}