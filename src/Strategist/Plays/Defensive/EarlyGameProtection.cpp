#include "EarlyGameProtection.h"

#include "Map.h"
#include "General.h"
#include "Units.h"

EarlyGameProtection::EarlyGameProtection() : squad(std::make_shared<DefendBaseSquad>(Map::getMyMain()))
{
    General::addSquad(squad);
}

void EarlyGameProtection::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Currently hard-coded to 2 zealots
    // TODO: Dynamically adjust number of zealots and priority based on scouting data
    int countToProduce = 2 - Units::countAll(BWAPI::UnitTypes::Protoss_Zealot);
    if (countToProduce > 0)
    {
        prioritizedProductionGoals[20].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Zealot, countToProduce, 1);
    }
}