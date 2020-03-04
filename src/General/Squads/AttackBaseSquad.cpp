#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Map.h"
#include "PathFinding.h"

void AttackBaseSquad::execute(UnitCluster &cluster)
{
    // Look for enemies near this cluster
    std::set<Unit> enemyUnits;
    Units::enemyInRadius(enemyUnits, cluster.center, 480);

    // If there are no enemies near the cluster, just move towards the target
    if (enemyUnits.empty())
    {
        cluster.setActivity(UnitCluster::Activity::Default);
        cluster.move(targetPosition);
        return;
    }

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);

    // Run combat sim
    auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits);

    // TODO: If our units can't do any damage (e.g. ground-only vs. air, melee vs. kiting ranged units), do something else

    // For now, press the attack if one of these holds:
    // - Attacking does not cost us anything
    // - We gain value without losing too much of our army
    // - We gain significant army proportion
    // - We have a huge advantage, so there's no reason to withdraw
    // TODO: Make this more dynamic, integrate more of the logic from old Locutus
    // TODO: Consider whether it is even more beneficial to wait for nearby reinforcements
    bool attack =
            simResult.myPercentLost() <= 0.001 ||
            (simResult.valueGain() > 0 && (simResult.percentGain() > -0.05 || simResult.myPercentageOfTotal() > 0.9)) ||
            simResult.percentGain() > 0.2;

#if DEBUG_COMBATSIM
    CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                     << ": %l=" << simResult.myPercentLost()
                     << "; vg=" << simResult.valueGain()
                     << "; %g=" << simResult.percentGain()
                     << (attack ? "" : "; RETREAT");
#endif

    if (attack)
    {
        cluster.attack(unitsAndTargets, targetPosition);
        return;
    }

    auto &grid = Players::grid(BWAPI::Broodwar->self());

    // Find a suitable retreat location:
    // - Unit furthest from the order position that is not under threat
    // - If no such unit exists, our main base
    // TODO: Avoid retreating through enemy army, proper pathing, link up with other clusters, etc.
    BWAPI::Position rallyPoint = Map::getMyMain()->getPosition();
    int bestDist = 0;
    for (auto &unit : cluster.units)
    {
        if (grid.groundThreat(unit->lastPosition) > 0) continue;

        int dist = PathFinding::GetGroundDistance(unit->lastPosition, targetPosition, unit->type);
        if (dist > bestDist)
        {
            bestDist = dist;
            rallyPoint = unit->lastPosition;
        }
    }

    cluster.regroup(rallyPoint);
}