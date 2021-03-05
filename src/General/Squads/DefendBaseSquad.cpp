#include "DefendBaseSquad.h"

void DefendBaseSquad::execute()
{
    Squad::execute();

    if (clusters.empty())
    {
        auto workersAndTargets = workerDefenseSquad->selectTargets(enemyUnits);
        std::vector<std::pair<MyUnit, Unit>> emptyUnitsAndTargets;
        workerDefenseSquad->execute(workersAndTargets, emptyUnitsAndTargets);
    }
}

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

    if (cluster.isVanguardCluster)
    {
        auto workersAndTargets = workerDefenseSquad->selectTargets(enemyUnits);
        workerDefenseSquad->execute(workersAndTargets, unitsAndTargets);
    }
}
