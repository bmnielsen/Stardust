#include "MopUpSquad.h"

#include "Units.h"
#include "PathFinding.h"
#include "Map.h"

MopUpSquad::MopUpSquad() : Squad("Mop Up")
{
    targetPosition = Map::getMyMain()->getPosition();
}

void MopUpSquad::execute(UnitCluster &cluster)
{
    // If there are enemy units near the cluster, attack them
    // TODO: Refactor so we can use the same code as in AttackBaseSquad (combat sim, etc.)
    std::set<std::shared_ptr<Unit>> enemyUnits;
    Units::getInRadius(enemyUnits, BWAPI::Broodwar->enemy(), cluster.center, 480);
    if (!enemyUnits.empty())
    {
        auto unitsAndTargets = cluster.selectTargets(enemyUnits, cluster.center);
        cluster.attack(unitsAndTargets, cluster.center);
        return;
    }

    // Search for the closest known enemy building to the cluster
    int closestDist = INT_MAX;
    BWAPI::Position closestPosition = BWAPI::Positions::Invalid;
    for (auto &enemyUnit : Units::getForPlayer(BWAPI::Broodwar->enemy()))
    {
        if (!enemyUnit->type.isBuilding()) continue;
        if (!enemyUnit->lastPositionValid || !enemyUnit->lastPosition.isValid()) continue;

        int dist = enemyUnit->isFlying
                   ? enemyUnit->lastPosition.getApproxDistance(cluster.center)
                   : PathFinding::GetGroundDistance(cluster.center, enemyUnit->lastPosition);
        if (dist < closestDist)
        {
            closestDist = dist;
            closestPosition = enemyUnit->lastPosition;
        }
    }

    // If we found one, move towards it
    if (closestPosition.isValid())
    {
        cluster.setActivity(UnitCluster::Activity::Default);
        cluster.move(closestPosition);
        return;
    }

    // We don't know of any enemy buildings, so try to find one
    // TODO: Make this based on when we last scouted tiles
    // TODO: Somehow handle terran floated buildings
    int bestBaseScoutedAt = INT_MAX;
    Base *bestBase = nullptr;
    for (auto &base : Map::allBases())
    {
        if (base->owner == BWAPI::Broodwar->self()) continue;

        if (base->lastScouted < bestBaseScoutedAt)
        {
            bestBaseScoutedAt = base->lastScouted;
            bestBase = base;
        }
    }

    if (bestBase)
    {
        cluster.setActivity(UnitCluster::Activity::Default);
        cluster.move(bestBase->getPosition());
        return;
    }
}
