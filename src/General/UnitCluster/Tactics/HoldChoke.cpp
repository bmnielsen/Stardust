#include "UnitCluster.h"

#include "Units.h"
#include "Players.h"
#include "Map.h"
#include "Geo.h"
#include "Boids.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

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

        // For ranged units, we attack when the unit is about to be in our weapon range
        if (UnitUtil::IsRangedUnit(myUnit->type))
        {
            auto predictedTargetPos = unitAndTarget.second->predictPosition(BWAPI::Broodwar->getLatencyFrames());
            if (!predictedTargetPos.isValid()) predictedTargetPos = unitAndTarget.second->lastPosition;
            if (!myUnit->isInOurWeaponRange(target, predictedTargetPos)) continue;

            rangedShouldAttack = true;

            // We also attack with melee units if we don't outrange the target
            if (myUnit->isInEnemyWeaponRange(target, predictedTargetPos, 16))
            {
                meleeShouldAttack = true;
                break;
            }

            continue;
        }

        // For melee units vs. ranged units, attack if the target has us in range
        if (UnitUtil::IsRangedUnit(target->type))
        {
            if (myUnit->isInEnemyWeaponRange(target, -16))
            {
                rangedShouldAttack = true;
                meleeShouldAttack = true;
                break;
            }

            continue;
        }

        // For melee units vs. melee units, attack if the target is about to be in range
        {
            auto predictedTargetPos = unitAndTarget.second->predictPosition(BWAPI::Broodwar->getLatencyFrames());
            if (!predictedTargetPos.isValid()) predictedTargetPos = unitAndTarget.second->lastPosition;
            if (!myUnit->isInOurWeaponRange(target, predictedTargetPos)) continue;

            rangedShouldAttack = true;
            meleeShouldAttack = true;
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

    std::vector<std::tuple<MyUnit, int, BWAPI::Position>> unitsAndMoveTargets;
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
            int distToCenter = myUnit->getDistance(choke->center);
            if (distToCenter < defendEndDist)
            {
                // Unit is on wrong side of the defend end, closer to choke center
                // If the unit is far away, use the choke center as the reference point
                targetPos = (defendEndDist > std::max(centerDist, 32) * 2) ? choke->center : defendEnd;
                distDiff = defendEndDist;
            }
            else if (distToCenter < centerDist)
            {
                // Unit is on wrong side of the defend end, closer to defend end
                targetPos = choke->center;
                distDiff = -defendEndDist - myUnit->groundRange();
            }
            else
            {
                // Normal defend boid
                targetPos = defendEnd;
                distDiff = defendEndDist - myUnit->groundRange();
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
            else if (unitAndTarget.second && myUnit->isInEnemyWeaponRange(unitAndTarget.second, 48))
            {
                // Unit is in its target's attack range
                targetPos = unitAndTarget.second->lastPosition;
                distDiff = myUnit->getDistance(unitAndTarget.second)
                           - unitAndTarget.second->groundRange()
                           - 48;
            }
            else
            {
                targetPos = choke->center;
                distDiff = myUnit->getDistance(targetPos) - meleeDist;
            }
        }

        unitsAndMoveTargets.emplace_back(std::make_tuple(myUnit, distDiff, targetPos));
    }

    // Now execute move orders
    auto navigationGrid = PathFinding::getNavigationGrid(BWAPI::TilePosition(choke->center));
    for (auto &moveUnit : unitsAndMoveTargets)
    {
        auto &myUnit = std::get<0>(moveUnit);
        auto distDiff = std::get<1>(moveUnit);
        auto targetPos = std::get<2>(moveUnit);

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
            auto otherDistDiff = std::abs(std::get<1>(otherMoveUnit));
            if (otherDistDiff < 5) continue;

            Boids::AddSeparation(myUnit.get(), other, separationDetectionLimitFactor, separationWeight, separationX, separationY);
        }

        auto pos = Boids::ComputePosition(myUnit.get(), {goalX, separationX}, {goalY, separationY}, 0);

#if DEBUG_UNIT_ORDERS
        CherryVis::log(myUnit->id) << "HoldChoke boids towards " << BWAPI::WalkPosition(defendEnd)
                                   << "; distDiff=" << distDiff
                                   << ": goal=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX, goalY))
                                   << "; separation=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(separationX, separationY))
                                   << "; total="
                                   << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX + separationX, goalY + separationY))
                                   << "; target=" << BWAPI::WalkPosition(pos);
#endif

        if (pos != BWAPI::Positions::Invalid)
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
