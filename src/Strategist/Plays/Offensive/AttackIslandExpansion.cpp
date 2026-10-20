#include "AttackIslandExpansion.h"

#include "General.h"
#include "Units.h"
#include "Map.h"

AttackIslandExpansion::AttackIslandExpansion(Base *baseLoc)
        : PlayItem((std::ostringstream() << "Attack island expansion @ " << baseLoc->getTilePosition()).str())
        , baseLoc(baseLoc)
        , squad(std::make_shared<AttackBaseSquad>(baseLoc))
{
    Marshal::addSquad(squad);
}

void AttackIslandExpansion::update()
{
    /* Complete the play when the base is no longer owned by the enemy */
    if (baseLoc->owner != BWAPI::Broodwar->enemy())
    {
        status.complete = true;
        return;
    }

    /* Update detection - release observers when no longer needed, request observers when needed */
    bool needDetection = squad->needsDetection() || UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Vulture_Spider_Mine);
    auto &detectors = squad->getDetectors();
    if (!needDetection && !detectors.empty())
    {
        status.removedUnits.insert(status.removedUnits.end(), detectors.begin(), detectors.end());
    }
    else if (needDetection && detectors.empty())
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());

        /* Release the squad units until we get the detector */
        status.removedUnits = squad->getUnits();
        return;
    }

    /* Take all carriers */
    status.unitRequirements.emplace_back(10, BWAPI::UnitTypes::Protoss_Carrier, Map::getMyMain()->getPosition());
}

void AttackIslandExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Build an observer if we need one */
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}
