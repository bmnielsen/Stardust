#include "UnitCluster.h"

#include "Units.h"
#include "Players.h"
#include "Map.h"
#include "Geo.h"

/*
 * Contains a base, here defined as somewhere the enemy has static defense.
 *
 * We basically order our units to fan out just outside of enemy threat range, attacking anything that comes into range of our units.
 */

namespace
{
    const int goalWeight = 64;
    const int goalWeightInRangeOfStaticDefense = 128;
    const double separationDetectionLimitFactor = 1.5;
    const double separationWeight = 96.0;
}

namespace
{
    bool isStaticDefense(BWAPI::UnitType type)
    {
        return type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
               type == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
               type == BWAPI::UnitTypes::Terran_Bunker ||
               type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
    }
}

void UnitCluster::containBase(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                              std::set<Unit> &enemyUnits,
                              BWAPI::Position targetPosition)
{
    auto &grid = Players::grid(BWAPI::Broodwar->enemy());
    auto navigationGrid = PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition));

    for (const auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = unitAndTarget.first;

        // If the unit is stuck, unstick it
        if (myUnit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit->isReady()) continue;

        // TODO: Reuse filtering of static defense from earlier
        bool inRangeOfStaticDefense = false;
        int closestStaticDefenseDist = INT_MAX;
        Unit closestStaticDefense = nullptr;
        for (auto &unit : enemyUnits)
        {
            if (!isStaticDefense(unit->type)) continue;

            int dist = myUnit->getDistance(unit);
            if (dist < closestStaticDefenseDist)
            {
                closestStaticDefenseDist = dist;
                closestStaticDefense = unit;
            }

            if (myUnit->isInEnemyWeaponRange(unit)) inRangeOfStaticDefense = true;
        }

        // If this unit is not in range of static defense and is in range of its target, attack
        if (unitAndTarget.second && !inRangeOfStaticDefense && myUnit->isInOurWeaponRange(unitAndTarget.second))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Contain: Attacking " << *unitAndTarget.second;
#endif
            myUnit->attackUnit(unitAndTarget.second, unitsAndTargets);
            continue;
        }

        // Move to maintain the contain

        // Goal boid: stay just out of range of enemy threats, oriented towards the target position
        bool pullingBack = false;
        int goalX = 0;
        int goalY = 0;

        // If in range of static defense, just move away from it
        if (inRangeOfStaticDefense)
        {
            auto scaled = Geo::ScaleVector(myUnit->lastPosition - closestStaticDefense->lastPosition,
                                           goalWeightInRangeOfStaticDefense);
            if (scaled != BWAPI::Positions::Invalid)
            {
                goalX = scaled.x;
                goalY = scaled.y;
            }
            pullingBack = true;
        }
        else
        {
            // Get grid nodes if they are available
            auto node = navigationGrid ? &(*navigationGrid)[myUnit->getTilePosition()] : nullptr;
            auto nextNode = node ? node->nextNode : nullptr;
            auto secondNode = nextNode ? nextNode->nextNode : nullptr;

            BWAPI::Position nextNodeCenter, secondNodeCenter;
            if (secondNode)
            {
                nextNodeCenter = nextNode->center();
                secondNodeCenter = secondNode->center();
            }
            else
            {
                // We hit this if we have no valid path to the base we are attacking, which might happen if the enemy walls it off
                // So we use a vector towards the next choke instead
                // If the next choke is very close, use the one after that instead
                auto path = PathFinding::GetChokePointPath(myUnit->lastPosition, targetPosition, myUnit->type);
                BWAPI::Position waypoint = BWAPI::Positions::Invalid;
                for (const auto &bwemChoke : PathFinding::GetChokePointPath(myUnit->lastPosition, targetPosition, myUnit->type))
                {
                    auto chokeCenter = Map::choke(bwemChoke)->center;
                    if (myUnit->getDistance(chokeCenter) > 128)
                    {
                        waypoint = chokeCenter;
                        break;
                    }
                }
                if (!waypoint.isValid()) waypoint = targetPosition;

                auto vector = waypoint - myUnit->lastPosition;
                nextNodeCenter = Geo::ScaleVector(vector, 32);
                secondNodeCenter = Geo::ScaleVector(vector, 64);
            }

            // Move towards the second node if none are under threat
            // Move away from the second node if the next node is under threat
            // Do nothing if the first node is not under threat and the second node is
            int length = goalWeight;
            if (grid.groundThreat(nextNodeCenter) > 0)
            {
                length = -goalWeight;
                pullingBack = true;
            }
            else if (grid.groundThreat(secondNodeCenter) > 0)
            {
                length = 0;
            }
            if (length != 0)
            {
                auto scaled = Geo::ScaleVector(secondNodeCenter - myUnit->lastPosition, length);
                if (scaled != BWAPI::Positions::Invalid)
                {
                    goalX = scaled.x;
                    goalY = scaled.y;
                }
            }
        }

        // Separation boid
        int separationX = 0;
        int separationY = 0;
        for (const auto &otherUnitAndTarget : unitsAndTargets)
        {
            if (otherUnitAndTarget.first == myUnit) continue;

            auto other = otherUnitAndTarget.first;

            auto dist = myUnit->getDistance(other);
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
        auto total = Geo::ScaleVector(BWAPI::Position(totalX, totalY), 80);
        auto pos = total == BWAPI::Positions::Invalid ? myUnit->lastPosition : (myUnit->lastPosition + total);

#if DEBUG_UNIT_ORDERS
        CherryVis::log(myUnit->id) << "Contain boids towards " << BWAPI::WalkPosition(targetPosition)
                                   << "; cluster=" << BWAPI::WalkPosition(center)
                                   << ": goal=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX, goalY))
                                   << "; separation=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(separationX, separationY))
                                   << "; total=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(totalX, totalY))
                                   << "; target=" << BWAPI::WalkPosition(pos);
#endif

        // If the position is invalid or unwalkable, either move towards the target or towards our main depending on whether we are pulling back
        if (!pos.isValid() || !Map::isWalkable(BWAPI::TilePosition(pos)))
        {
            myUnit->moveTo(pullingBack ? Map::getMyMain()->getPosition() : targetPosition);
        }
        else
        {
            myUnit->move(pos, true);
        }
    }

}
