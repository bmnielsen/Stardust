#include "UnitCluster.h"

#include "Units.h"
#include "Players.h"
#include "Map.h"
#include "Geo.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

/*
 * Holds a choke.
 *
 * Basic idea:
 * - Melee units hold the line at the choke end
 * - Ranged units keep in range of a point further inside the choke
 */

namespace
{
    const int goalWeight = 96;
    const double separationDetectionLimitFactor = 1.0;
    const double separationWeight = 64;
}

void UnitCluster::holdChoke(Choke *choke,
                            BWAPI::Position defendEnd,
                            std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets)
{
    // Basic idea:
    // - Melee units hold the line at the choke end
    // - Ranged units keep in range of the choke end

    BWAPI::Position farEnd = choke->end1Center == defendEnd ? choke->end2Center : choke->end1Center;
    int centerDist = choke->center.getApproxDistance(defendEnd);
    int meleeDist = std::max(choke->width / 2, centerDist);

    // On the first pass, determine which types of units should attack
    bool meleeShouldAttack = false;
    bool rangedShouldAttack = false;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        auto &target = unitAndTarget.second;
        if (!target) continue;

        auto &myUnit = unitAndTarget.first;

        // TODO: Check if the unit is in position

        // Determine if this unit should be attacked

        // If the target is close enough to the defend end, attack with all units
        if (target->getDistance(defendEnd) <= std::max(choke->width / 2,
                                                       std::min(myUnit->groundRange(), centerDist)))
        {
            meleeShouldAttack = true;
            rangedShouldAttack = true;
            break;
        }

        // If the target is about to be in our weapon range, attack with ranged or all units
        auto predictedTargetPos = unitAndTarget.second->predictPosition(BWAPI::Broodwar->getLatencyFrames());
        if (!predictedTargetPos.isValid()) predictedTargetPos = unitAndTarget.second->lastPosition;
        if (!myUnit->isInOurWeaponRange(target, predictedTargetPos)) continue;
        if (UnitUtil::IsRangedUnit(myUnit->type))
        {
            rangedShouldAttack = true;

            // If we don't outrange the target, attack with melee units as well
            if (myUnit->isInEnemyWeaponRange(target, predictedTargetPos, 16))
            {
                meleeShouldAttack = true;
                break;
            }
        }
        else
        {
            meleeShouldAttack = true;
            rangedShouldAttack = true;
            break;
        }
    }

    // On the next pass, attack with any units that need to and collect the units that need to move
    // TODO: Include determination of whether the unit is in position
    auto shouldAttack = [&](const MyUnit &myUnit, const Unit &target)
    {
        // Special logic for dragoons on cooldown
        if (myUnit->type == BWAPI::UnitTypes::Protoss_Dragoon &&
            myUnit->cooldownUntil > (BWAPI::Broodwar->getFrameCount() + BWAPI::Broodwar->getRemainingLatencyFrames() + 2))
        {
            // "Attack" to activate our kiting logic if we are close to being in the target's weapon range
            // Otherwise we fall through to choke boid movement
            int targetRange = Players::weaponDamage(target->player, target->type.groundWeapon());
            return myUnit->getDistance(target) < (targetRange + 64);
        }

        return UnitUtil::IsRangedUnit(myUnit->type) ? rangedShouldAttack : meleeShouldAttack;
    };

    std::vector<std::tuple<MyUnit, BWAPI::Position, int, BWAPI::Position>> unitsAndMoveTargets;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = unitAndTarget.first;

        // If the unit is stuck, unstick it
        if (myUnit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit->isReady()) continue;

        int distToChokeCenter = PathFinding::GetGroundDistance(myUnit->lastPosition,
                                                               choke->center,
                                                               myUnit->type,
                                                               PathFinding::PathFindingOptions::UseNearestBWEMArea);

        // Attack
        if (unitAndTarget.second && shouldAttack(myUnit, unitAndTarget.second))
        {
            // If the target is not in our weapon range, use move logic instead if we are a long way from the choke
            if (distToChokeCenter > 300 && !myUnit->isInOurWeaponRange(unitAndTarget.second))
            {
                myUnit->moveTo(choke->center);
                continue;
            }

#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "HoldChoke: Attacking " << *unitAndTarget.second;
#endif
            myUnit->attackUnit(unitAndTarget.second, unitsAndTargets, false);
            continue;
        }

        // If the unit is a long way away from the choke, move towards it instead so our pathing kicks in
        if (distToChokeCenter > 300)
        {
            myUnit->moveTo(choke->center);
            continue;
        }

        // This unit should move, so add it to the vector

        // Determine the position this unit is keeping its distance from
        BWAPI::Position targetPos;
        int distDiff;

        int defendEndDist = myUnit->getDistance(defendEnd);

        if (UnitUtil::IsRangedUnit(myUnit->type))
        {
            int targetDist = myUnit->getDistance(defendEnd);
            int distToCenter = myUnit->getDistance(choke->center);
            if (distToCenter < targetDist)
            {
                // Unit is on wrong side of the defend end, closer to choke center
                targetPos = defendEnd;
                distDiff = targetDist;
            }
            else if (distToCenter < centerDist)
            {
                // Unit is on wrong side of the defend end, closer to defend end
                targetPos = choke->center;
                distDiff = -targetDist - myUnit->groundRange();
            }
            else
            {
                // Normal defend boid
                targetPos = defendEnd;
                distDiff = targetDist - myUnit->groundRange();
            }
        }
        else
        {
            if (myUnit->getDistance(farEnd) < defendEndDist)
            {
                // Unit is on the wrong side of the choke
                targetPos = defendEnd;
                distDiff = defendEndDist;
            }
            else if (unitAndTarget.second && myUnit->isInEnemyWeaponRange(unitAndTarget.second, 32))
            {
                // Unit is in its target's attack range
                targetPos = unitAndTarget.second->lastPosition;
                distDiff = myUnit->getDistance(unitAndTarget.second)
                           - unitAndTarget.second->groundRange()
                           - 32;
            }
            else
            {
                targetPos = choke->center;
                distDiff = myUnit->getDistance(targetPos) - meleeDist;
            }
        }

        auto predictedPosition = myUnit->predictPosition(BWAPI::Broodwar->getLatencyFrames());
        if (!predictedPosition.isValid()) predictedPosition = myUnit->lastPosition;

        unitsAndMoveTargets.emplace_back(std::make_tuple(myUnit, predictedPosition, distDiff, targetPos));
    }

    // Now execute move orders
    auto navigationGrid = PathFinding::getNavigationGrid(BWAPI::TilePosition(choke->center));
    for (auto &moveUnit : unitsAndMoveTargets)
    {
        auto &myUnit = std::get<0>(moveUnit);
        auto predictedPosition = std::get<1>(moveUnit);
        auto distDiff = std::get<2>(moveUnit);
        auto targetPos = std::get<3>(moveUnit);

        // Move to maintain the contain
        int goalX = 0;
        int goalY = 0;
        {
            // Get position to move to, either towards or away from the target position
            BWAPI::Position scaled;
            if (distDiff > 0)
            {
                // Use the grid node to get to the choke center
                NavigationGrid::GridNode *currentNode = nullptr;
                NavigationGrid::GridNode *nextNode = nullptr;
                if (targetPos == choke->center)
                {
                    currentNode = navigationGrid ? &(*navigationGrid)[myUnit->getTilePosition()] : nullptr;
                    nextNode = currentNode ? currentNode->nextNode : nullptr;
                }

                if (nextNode)
                {
                    scaled = Geo::ScaleVector(nextNode->center() - currentNode->center(), std::min(goalWeight, distDiff));
                }
                else
                {
                    scaled = Geo::ScaleVector(targetPos - myUnit->lastPosition, std::min(goalWeight, distDiff));
                }
            }
            else if (distDiff < 0)
            {
                scaled = Geo::ScaleVector(myUnit->lastPosition - targetPos, std::min(goalWeight, -distDiff));
            }

            if (scaled != BWAPI::Positions::Invalid)
            {
                goalX = scaled.x;
                goalY = scaled.y;
            }
        }

        // Separation boid
        int separationX = 0;
        int separationY = 0;
        for (const auto &otherMoveUnit : unitsAndMoveTargets)
        {
            auto &other = std::get<0>(otherMoveUnit);
            if (myUnit == other) continue;

            // Don't move out of the way of units already at their desired position
            auto otherDistDiff = std::abs(std::get<2>(otherMoveUnit));
            if (otherDistDiff < 5) continue;

            auto otherPredictedPosition = std::get<1>(otherMoveUnit);
            auto dist = Geo::EdgeToEdgeDistance(myUnit->type,
                                                predictedPosition,
                                                other->type,
                                                otherPredictedPosition);
            double detectionLimit = std::max(myUnit->type.width(), other->type.width()) * separationDetectionLimitFactor;
            if (dist >= (int) detectionLimit) continue;

            // We are within the detection limit
            // Push away with maximum force at 0 distance, no force at detection limit
            double distFactor = 1.0 - (double) dist / detectionLimit;
            auto vector = Geo::ScaleVector(myUnit->lastPosition - other->lastPosition,
                                           (int) (distFactor * distFactor * separationWeight));
            if (vector != BWAPI::Positions::Invalid)
            {
                separationX += vector.x;
                separationY += vector.y;
            }
        }

        // Put them all together to get the target direction
        int totalX = goalX + separationX;
        int totalY = goalY + separationY;

        // If the unit is in a no-go area, get out of it immediately
        // TODO: This needs to be refactored to be usable from all tactics
        if (Map::isInNoGoArea(myUnit->tilePositionX, myUnit->tilePositionY))
        {
            // Find the closest tile that is walkable and not in a no-go area
            int closestDist = INT_MAX;
            for (int x = myUnit->tilePositionX - 5; x < myUnit->tilePositionX + 5; x++)
            {
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

                for (int y = myUnit->tilePositionY - 5; y < myUnit->tilePositionY + 5; y++)
                {
                    if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;
                    if (!Map::isWalkable(x, y)) continue;
                    if (Map::isInNoGoArea(x, y)) continue;

                    int dist = Geo::ApproximateDistance(myUnit->tilePositionX, x, myUnit->tilePositionY, y);
                    if (dist < closestDist)
                    {
                        closestDist = dist;
                        totalX = (x - myUnit->tilePositionX) * 32;
                        totalY = (y - myUnit->tilePositionY) * 32;
                    }
                }
            }
        }

        // Find a walkable position along the vector
        auto totalVector = BWAPI::Position(totalX, totalY);
        auto pos = Geo::WalkablePositionAlongVector(myUnit->lastPosition, totalVector);

#if DEBUG_UNIT_ORDERS
        CherryVis::log(myUnit->id) << "HoldChoke boids towards " << BWAPI::WalkPosition(defendEnd)
                                   << "; distDiff=" << distDiff
                                   << ": goal=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX, goalY))
                                   << "; separation=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(separationX, separationY))
                                   << "; total=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(totalX, totalY))
                                   << "; target=" << BWAPI::WalkPosition(pos);
#endif

        if (pos.isValid() && BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos)))
        {
            myUnit->moveTo(pos, true);
            continue;
        }

        // In cases where the position is not walkable, try with the goal position instead
        pos = myUnit->lastPosition + BWAPI::Position(goalX, goalY);
        if (pos.isValid() && BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos)))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Target not walkable; falling back to goal boid";
#endif
            myUnit->moveTo(pos, true);
            continue;
        }

        // If the position is still invalid or unwalkable, move directly to the target position or back depending on which way we are going
        if (distDiff > 0)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Target not walkable; moving directly to target";
#endif
            myUnit->moveTo(targetPos, true);
        }
        else if (distDiff < 0)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Target not walkable; pulling back";
#endif
            // This isn't really correct for ranged units, but we don't expect them to be getting invalid positions often
            myUnit->moveTo(defendEnd, true);
        }
        else
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Target not walkable; staying put";
#endif
            myUnit->moveTo(myUnit->lastPosition, true);
        }
    }
}
