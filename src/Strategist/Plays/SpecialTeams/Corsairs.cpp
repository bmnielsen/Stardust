#include "Corsairs.h"

#include "Map.h"
#include "Players.h"

Corsairs::Corsairs(): Play("Corsairs"), squad(std::make_shared<CorsairSquad>())
{
    General::addSquad(squad);
}

void Corsairs::update()
{
    // Always request corsairs so all created corsairs get assigned to this play
    status.unitRequirements.emplace_back(10, BWAPI::UnitTypes::Protoss_Corsair, Map::getMyMain()->getPosition());
}

void Corsairs::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Enable when we can use disruption web
//    if (squad->combatUnitCount() >= 5
//        && !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Disruption_Web)
//        && !Units::isBeingUpgradedOrResearched(BWAPI::TechTypes::Disruption_Web))
//    {
//        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
//                                                                 label,
//                                                                 BWAPI::TechTypes::Disruption_Web);
//
//    }
}
