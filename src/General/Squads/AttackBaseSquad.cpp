#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Map.h"
#include "PathFinding.h"

void AttackBaseSquad::execute(UnitCluster &cluster)
{
    // Look for enemies near this cluster
    std::set<std::shared_ptr<Unit>> enemyUnits;
    Units::getInRadius(enemyUnits, BWAPI::Broodwar->enemy(), cluster.center, 480);

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);

    // Run combat sim
    auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits);

    // For now, press the attack if one of these holds:
    // - Attacking does not cost us anything
    // - We gain value without losing too much of our army
    // - We gain significant army proportion
    // TODO: Make this more dynamic, integrate more of the logic from old Locutus
    // TODO: Consider whether it is even more beneficial to wait for nearby reinforcements
    bool attack =
            simResult.myPercentLost() <= 0.001 ||
            (simResult.valueGain() > 0 && simResult.proportionalGain() > -0.05) ||
            simResult.proportionalGain() > 0.2;

#ifdef DEBUG_COMBATSIM
    CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                     << ": %l=" << simResult.myPercentLost()
                     << "; vg=" << simResult.valueGain()
                     << "; %g=" << simResult.proportionalGain()
                     << (attack ? "" : "; RETREAT");
#endif

    if (attack)
    {
        cluster.execute(unitsAndTargets, targetPosition);
        return;
    }

    auto & grid = Players::grid(BWAPI::Broodwar->self());

    // Find a suitable retreat location:
    // - Unit furthest from the order position that is not under threat
    // - If no such unit exists, our main base
    // TODO: Avoid retreating through enemy army, proper pathing, link up with other clusters, etc.
    BWAPI::Position rallyPoint = Map::getMyMain()->getPosition();
    int bestDist = 0;
    for (auto & unit : cluster.units)
    {
        if (grid.groundThreat(unit->getPosition()) > 0) continue;

        int dist = PathFinding::GetGroundDistance(unit->getPosition(), targetPosition, unit->getType());
        if (dist > bestDist)
        {
            bestDist = dist;
            rallyPoint = unit->getPosition();
        }
    }

    // Micro each unit
    for (auto & unit : cluster.units)
    {
        auto & myUnit = Units::getMine(unit);

        // If the unit is stuck, unstick it
        if (myUnit.isStuck())
        {
            myUnit.stop();
            continue;
        }

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit.isReady()) continue;

        myUnit.moveTo(rallyPoint);
    }
}