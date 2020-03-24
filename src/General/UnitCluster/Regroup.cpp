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
 *
 * TODO: Consider how a fleeing cluster should move (does it make sense to use flocking?)
 */

namespace
{
    bool isStaticDefense(BWAPI::UnitType type)
    {
        return type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
               type == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
               type == BWAPI::UnitTypes::Terran_Bunker ||
               type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
    }

    bool hasStaticDefense(std::set<Unit> &enemyUnits)
    {
        for (const auto &unit : enemyUnits)
        {
            if (isStaticDefense(unit->type))
            {
                return true;
            }
        }

        return false;
    }

    bool shouldContain(UnitCluster &cluster, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, std::set<Unit> &enemyUnits)
    {
        if (!hasStaticDefense(enemyUnits)) return false;

        // Run a combat sim excluding the enemy static defense
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

    void regroupToPosition(UnitCluster &cluster, BWAPI::Position position)
    {
        for (const auto &unit : cluster.units)
        {
            // If the unit is stuck, unstick it
            if (unit->unstick()) continue;

            // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
            if (!unit->isReady()) continue;

#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Regroup: Moving to " << BWAPI::WalkPosition(position);
#endif
            unit->moveTo(position);
        }
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

    if (currentSubActivity == SubActivity::Contain)
    {
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());

        // Temporary logic: regroup to either the unit closest to or furthest from to the order position that isn't under threat
        MyUnit closest = nullptr;
        MyUnit furthest = nullptr;
        int closestDist = INT_MAX;
        int furthestDist = 0;
        bool unitUnderThreat = false;
        for (auto &unit : units)
        {
            if (grid.groundThreat(unit->lastPosition) > 0)
            {
                unitUnderThreat = true;
                continue;
            }

            int dist = PathFinding::GetGroundDistance(unit->lastPosition, targetPosition, unit->type);
            if (dist < closestDist)
            {
                closestDist = dist;
                closest = unit;
            }
            if (dist > furthestDist)
            {
                furthestDist = dist;
                furthest = unit;
            }
        }

        auto regroupPosition = Map::getMyMain()->getPosition();
        if (unitUnderThreat && furthest) regroupPosition = furthest->lastPosition;
        else if (!unitUnderThreat && closest) regroupPosition = closest->lastPosition;

        regroupToPosition(*this, regroupPosition);
    }
}
