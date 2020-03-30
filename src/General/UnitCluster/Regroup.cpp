#include "UnitCluster.h"

#include "Units.h"
#include "Players.h"
#include "Map.h"
#include "Geo.h"
#include "General.h"

/*
 * Orders a cluster to regroup.
 *
 * There are several regrouping strategies that can be chosen based on the situation:
 * - Contain: The enemy is considered to be mainly static, so retreat to a safe distance and attack anything that comes into range.
 * - Pull back: Retreat to a location that is more easily defensible (choke, high ground) and set up appropriately.
 * - Flee: Move back towards our main base until we are reinforced.
 * - Explode: Send the cluster units in multiple directions to confuse the enemy.
 */

namespace
{
    const int goalWeight = 64;
    const int goalWeightInStaticDefense = 128;
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

    bool shouldContain(UnitCluster &cluster, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, std::set<Unit> &enemyUnits)
    {
        // Run a combat sim excluding enemy static defense
        std::vector<std::pair<MyUnit, Unit>> filteredUnitsAndTargets;
        std::set<Unit> filteredEnemyUnits;
        for (const auto &pair : unitsAndTargets)
        {
            if (pair.second && !isStaticDefense(pair.second->type))
            {
                filteredUnitsAndTargets.emplace_back(pair);
            }
            else
            {
                filteredUnitsAndTargets.emplace_back(std::make_pair(pair.first, nullptr));
            }
        }
        for (auto &unit : enemyUnits)
        {
            if (!isStaticDefense(unit->type)) filteredEnemyUnits.insert(unit);
        }

        auto simResult = cluster.runCombatSim(filteredUnitsAndTargets, filteredEnemyUnits);
        simResult.setAttacking(false);

        bool contain = simResult.myPercentLost() <= 0.001 ||
                       (simResult.valueGain() > 0 && simResult.percentGain() > -0.05) ||
                       simResult.percentGain() > 0.2;

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << (contain ? "; CONTAIN" : "; FLEE");
#endif

        return contain;
    }
}

void UnitCluster::regroup(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, std::set<Unit> &enemyUnits, BWAPI::Position targetPosition)
{
    // Consider whether to contain (or continue a contain)
    if (currentSubActivity == SubActivity::None || currentSubActivity == SubActivity::Contain)
    {
        if (shouldContain(*this, unitsAndTargets, enemyUnits))
        {
            setSubActivity(SubActivity::Contain);
        }
        else
        {
            setSubActivity(SubActivity::Flee);
        }
    }

    if (currentSubActivity == SubActivity::Flee)
    {
        // TODO: Support fleeing elsewhere
        move(Map::getMyMain()->getPosition());
        return;
    }

    // TODO: Separate logic for containing a choke

    if (currentSubActivity == SubActivity::Contain)
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
                auto scaled = Geo::ScaleVector(myUnit->lastPosition - closestStaticDefense->lastPosition, goalWeightInStaticDefense);
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

                if (secondNode)
                {
                    // Move towards the second node if none are under threat
                    // Move away from the second node if the next node is under threat
                    // Do nothing if the first node is not under threat and the second node is
                    int length = goalWeight;
                    if (grid.groundThreat(nextNode->center()) > 0)
                    {
                        length = -goalWeight;
                        pullingBack = true;
                    }
                    else if (grid.groundThreat(secondNode->center()) > 0)
                    {
                        length = 0;
                    }
                    if (length != 0)
                    {
                        auto scaled = Geo::ScaleVector(secondNode->center() - myUnit->lastPosition, length);
                        if (scaled != BWAPI::Positions::Invalid)
                        {
                            goalX = scaled.x;
                            goalY = scaled.y;
                        }
                    }
                }
                else
                {
                    // TODO: Implement a fallback if needed (use next choke?)
                    Log::Get() << "WARNING: Trying to do contain movement without navigation grid! "
                               << *myUnit << "; targetPosition=" << BWAPI::WalkPosition(targetPosition);
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
                auto vector = Geo::ScaleVector(myUnit->lastPosition - other->lastPosition, (int) (distFactor * distFactor * separationWeight));
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
}
