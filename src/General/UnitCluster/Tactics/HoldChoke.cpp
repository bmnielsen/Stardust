#include "UnitCluster.h"

#include "Units.h"
#include "Players.h"
#include "Map.h"
#include "Geo.h"
#include "UnitUtil.h"

/*
 * Holds a choke.
 *
 * Basic idea:
 * - Melee units hold the line at the choke end
 * - Ranged units keep in range of a point further inside the choke
 *
 * TODO: Needs different logic when attacker has higher-range units than we do - e.g. keep distance and pounce when the enemy starts crossing
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
    // - Ranged units keep in range of a point further inside the choke

    BWAPI::Position farEnd = choke->end1Center == defendEnd ? choke->end2Center : choke->end1Center;

    BWAPI::Position rangedTarget;
    {
        BWAPI::Position aheadOfEnd = defendEnd - Geo::ScaleVector(defendEnd - choke->center, 64);
        BWAPI::Position aheadOfCenter = choke->center - Geo::ScaleVector(choke->center - farEnd, 32);
        if (center.getApproxDistance(aheadOfEnd) < center.getApproxDistance(aheadOfCenter))
        {
            rangedTarget = aheadOfEnd;
        }
        else
        {
            rangedTarget = aheadOfCenter;
        }
    }

    int centerDist = choke->center.getApproxDistance(defendEnd);
    int meleeDist = std::max(choke->width / 2, centerDist);

    // On the first pass, determine if we should attack
    // We do this if any of our units are in range of their target
    bool attack = false;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        if (!unitAndTarget.second) continue;

        auto predictedTargetPos = unitAndTarget.second->predictPosition(BWAPI::Broodwar->getLatencyFrames());
        if (unitAndTarget.first->isInOurWeaponRange(unitAndTarget.second, predictedTargetPos))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log() << "HoldChoke: Attacking as " << *unitAndTarget.second << " will be in range of " << *unitAndTarget.first;
#endif
            attack = true;
            break;
        }
    }

    // On the next pass, attack with any units that need to, and collect the units that need to move
    auto shouldAttack = [&](const MyUnit &myUnit, const Unit &target)
    {
        // Dragoons don't attack while on cooldown
        if (myUnit->type == BWAPI::UnitTypes::Protoss_Dragoon)
        {
            int cooldown = target->isFlying ? myUnit->bwapiUnit->getAirWeaponCooldown() : myUnit->bwapiUnit->getGroundWeaponCooldown();
            if (cooldown > BWAPI::Broodwar->getRemainingLatencyFrames() + 2) return false;
        }

        int range = Players::weaponRange(myUnit->player, myUnit->type.groundWeapon());

        // Always attack if the enemy is in range
        // TODO: Should not attack if an orphaned unit caught in the middle or other end of the choke
        if (myUnit->getDistance(target) <= range) return true;

        // Ranged units attack if their target is in range of the defend end of the choke
        if (UnitUtil::IsRangedUnit(myUnit->type))
        {
            return target->getDistance(defendEnd) <= range;
        }

        // Melee units attack if their target is close enough to the defend end
        return target->getDistance(defendEnd) <= std::min(range, centerDist);
    };
    std::vector<std::tuple<MyUnit, BWAPI::Position, int, BWAPI::Position>> unitsAndMoveTargets;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = unitAndTarget.first;

        // If the unit is stuck, unstick it
        if (myUnit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit->isReady()) continue;

        // Attack
        if (unitAndTarget.second && attack && shouldAttack(myUnit, unitAndTarget.second))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "HoldChoke: Attacking " << *unitAndTarget.second;
#endif
            myUnit->attackUnit(unitAndTarget.second, unitsAndTargets, false);
            return;
        }

        // This unit should move, so add it to the vector

        // Determine the position this unit is keeping its distance from
        BWAPI::Position targetPos;
        int distDiff;

        int defendEndDist = myUnit->getDistance(defendEnd);

        if (UnitUtil::IsRangedUnit(myUnit->type))
        {
            int targetDist = myUnit->getDistance(rangedTarget);
            if (targetDist < defendEndDist)
            {
                // Unit is on the wrong side of the choke
                targetPos = defendEnd;
                distDiff = defendEndDist;
            }
            else
            {
                targetPos = rangedTarget;
                distDiff = targetDist - Players::weaponRange(myUnit->player, myUnit->type.groundWeapon());
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
            else if (unitAndTarget.second && unitAndTarget.second->isInOurWeaponRange(myUnit))
            {
                // Unit is in its target's attack range
                targetPos = unitAndTarget.second->lastPosition;
                distDiff = myUnit->getDistance(unitAndTarget.second)
                           - Players::weaponRange(unitAndTarget.second->player, unitAndTarget.second->type.groundWeapon());
            }
            else
            {
                targetPos = choke->center;
                distDiff = myUnit->getDistance(targetPos) - meleeDist;
            }
        }

        unitsAndMoveTargets.emplace_back(std::make_tuple(myUnit,
                                                         myUnit->predictPosition(BWAPI::Broodwar->getLatencyFrames()),
                                                         distDiff,
                                                         targetPos));
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
                auto node = navigationGrid ? &(*navigationGrid)[myUnit->getTilePosition()] : nullptr;
                auto nextNode = node ? node->nextNode : nullptr;
                if (nextNode)
                {
                    scaled = Geo::ScaleVector(nextNode->center() - node->center(), std::min(goalWeight, distDiff));
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
        auto totalVector = BWAPI::Position(totalX, totalY);

        // Find a walkable position along the vector
        int dist = Geo::ApproximateDistance(0, totalX, 0, totalY) - 16;
        auto pos = myUnit->lastPosition + totalVector;
        while (dist > 0 && (!pos.isValid() || !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))))
        {
            pos = myUnit->lastPosition + Geo::ScaleVector(totalVector, dist);
            dist -= 16;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(myUnit->id) << "HoldChoke boids towards " << BWAPI::WalkPosition(rangedTarget)
                                   << "; distDiff=" << distDiff
                                   << ": goal=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX, goalY))
                                   << "; separation=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(separationX, separationY))
                                   << "; total=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(totalX, totalY))
                                   << "; target=" << BWAPI::WalkPosition(pos);
#endif

        if (pos.isValid() && BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos)))
        {
            myUnit->move(pos, true);
            continue;
        }

        // In cases where the position is not walkable, try with the goal position instead
        pos = myUnit->lastPosition + BWAPI::Position(goalX, goalY);
        if (pos.isValid() && BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos)))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Target not walkable; falling back to goal boid";
#endif
            myUnit->move(pos, true);
            continue;
        }

        // If the position is still invalid or unwalkable, move directly to the target position or back depending on which way we are going
        if (distDiff > 0)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Target not walkable; moving directly to target";
#endif
            myUnit->move(targetPos, true);
        }
        else if (distDiff < 0)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Target not walkable; pulling back";
#endif
            // This isn't really correct for ranged units, but we don't expect them to be getting invalid positions often
            myUnit->move(defendEnd, true);
        }
        else
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Target not walkable; staying put";
#endif
            myUnit->move(myUnit->lastPosition, true);
        }
    }
}
