#include "DefendBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"

void DefendBaseSquad::setTargetPosition()
{
    // If the base is our main, defend at the single main choke (if there is one)
    if (base == Map::getMyMain())
    {
        // Find a single unblocked choke out of the main
        Choke *choke = nullptr;
        for (auto bwemChoke : base->getArea()->ChokePoints())
        {
            if (bwemChoke->Blocked()) continue;
            if (choke)
            {
                choke = nullptr;
                break;
            }
            choke = Map::choke(bwemChoke);
        }

        if (choke)
        {
            targetPosition = choke->Center();
            return;
        }
    }

    // If this base is our natural, defend at the single natural choke (if there is one)
    if (base == Map::getMyNatural())
    {
        // Find a single unblocked choke out of the natural that doesn't go to our main
        auto mainArea = Map::getMyMain()->getArea();
        Choke *choke = nullptr;
        for (auto bwemChoke : base->getArea()->ChokePoints())
        {
            if (bwemChoke->Blocked()) continue;
            if (bwemChoke->GetAreas().first == mainArea || bwemChoke->GetAreas().second == mainArea) continue;
            if (choke)
            {
                choke = nullptr;
                break;
            }
            choke = Map::choke(bwemChoke);
        }

        if (choke)
        {
            targetPosition = choke->Center();
            return;
        }
    }

    // By default we defend at the resource depot
    targetPosition = base->getPosition();
}

void DefendBaseSquad::execute(UnitCluster &cluster)
{
    // TODO: Update target position (run combat sim?)

    std::set<std::shared_ptr<Unit>> enemyUnits;

    // Get enemy combat units in our base
    Units::getInArea(enemyUnits, BWAPI::Broodwar->enemy(), Map::getMyMain()->getArea(), [](const std::shared_ptr<Unit> &unit)
    {
        return UnitUtil::IsCombatUnit(unit->type);
    });

    // Get enemy combat units very close to the target position
    Units::getInRadius(enemyUnits, BWAPI::Broodwar->enemy(), targetPosition, 64);

    // Execute
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);
    cluster.execute(unitsAndTargets, targetPosition);
}
