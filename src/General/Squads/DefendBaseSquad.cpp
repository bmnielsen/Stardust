#include "DefendBaseSquad.h"

void DefendBaseSquad::execute()
{
    for (auto iterRef = enemyUnits.begin(); iterRef != enemyUnits.end();)
    {
        if ((*iterRef)->exists())
        {
            iterRef++;
        }
        else
        {
            iterRef = enemyUnits.erase(iterRef);
        }
    }

    UnitSquad::execute();

    if (clusters.empty())
    {
        auto workersAndTargets = workerDefenseSquad->selectTargets(enemyUnits);
        std::vector<std::pair<OwnedUnit, Unit>> emptyUnitsAndTargets;
        workerDefenseSquad->execute(workersAndTargets, emptyUnitsAndTargets);
    }
}

void DefendBaseSquad::execute(UnitCluster &clump)
{
    if (enemyUnits.empty())
    {
        clump.setActivity(UnitCluster::Activity::Moving);
        clump.move(targetPosition);
        return;
    }

    updateDetectionNeeds(enemyUnits);

    /* Select targets */
    auto unitsAndTargets = clump.selectTargets(enemyUnits, targetPosition);

    /* Run combat sim */
    /* auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits, detectors, false); */

    /* TODO: Interpret result */

    clump.setActivity(UnitCluster::Activity::Attacking);
    clump.attack(unitsAndTargets, targetPosition);

    if (clump.isVanguardCluster)
    {
        auto workersAndTargets = workerDefenseSquad->selectTargets(enemyUnits);
        workerDefenseSquad->execute(workersAndTargets, unitsAndTargets);
    }
}
