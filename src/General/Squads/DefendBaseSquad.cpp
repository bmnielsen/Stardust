#include "DefendBaseSquad.h"

void DefendBaseSquad::execute(UnitCluster &cluster)
{
    if (enemyUnits.empty())
    {
        cluster.setActivity(UnitCluster::Activity::Moving);
        cluster.move(targetPosition);
        return;
    }

    updateDetectionNeeds(enemyUnits);

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);

    // Run combat sim
    // auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits, detectors, false);

    // TODO: Interpret result

    cluster.setActivity(UnitCluster::Activity::Attacking);
    cluster.attack(unitsAndTargets, targetPosition);
}
