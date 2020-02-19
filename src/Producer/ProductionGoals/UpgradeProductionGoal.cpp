#include "ProductionGoal.h"

#include "Units.h"

bool UpgradeProductionGoal::isFulfilled()
{
    // Fulfilled if we have already upgraded to the desired level
    if (BWAPI::Broodwar->self()->getUpgradeLevel(type) >= level) return true;

    // Fulfilled if something is currently upgrading it
    // Technically we still may want to go further, but we don't need to plan that in advance
    for (const auto &unit : Units::allMine())
    {
        if (!unit->completed) continue;
        if (unit->type != type.whatUpgrades()) continue;

        if (unit->bwapiUnit->isUpgrading() && unit->bwapiUnit->getUpgrade() == type) return true;
    }

    return false;
}

BWAPI::UnitType UpgradeProductionGoal::prerequisiteForNextLevel()
{
    if (isFulfilled()) return BWAPI::UnitTypes::None;

    return type.whatsRequired(BWAPI::Broodwar->self()->getUpgradeLevel(type) + 1);
}
