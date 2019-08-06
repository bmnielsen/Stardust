#include "EarlyGameProtection.h"

#include "Map.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }

EarlyGameProtection::EarlyGameProtection() : squad(std::make_shared<DefendBaseSquad>(Map::getMyMain()))
{
    General::addSquad(squad);
}

void EarlyGameProtection::update()
{
    // We stop requiring early game protection when we have more than two completed gateway units and there are no enemy combat units in our base
    if ((Units::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon)) > 2)
    {
        // Get enemy combat units in our base
        std::set<std::shared_ptr<Unit>> enemyCombatUnits;
        Units::getInArea(enemyCombatUnits, BWAPI::Broodwar->enemy(), Map::getMyMain()->getArea(), [](const std::shared_ptr<Unit>& unit) {
            return UnitUtil::IsCombatUnit(unit->type);
        });

        if (enemyCombatUnits.empty())
        {
            status.removedUnits = squad->getUnits();
            status.complete = true;
            return;
        }
    }

    // Otherwise make sure we have two zealots
    int zealotsNeeded = 2 - squad->getUnits().size(); // TODO: This is not very efficient
    if (zealotsNeeded > 0)
    {
        status.unitRequirements.emplace_back(zealotsNeeded, BWAPI::UnitTypes::Protoss_Zealot, squad->getTargetPosition());
    }
}

void EarlyGameProtection::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    for (auto & unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioritizedProductionGoals[20].emplace_back(std::in_place_type<UnitProductionGoal>, unitRequirement.type, unitRequirement.count, 1);
    }
}
