#include "ProductionGoal.h"

bool UpgradeProductionGoal::isFulfilled()
{
    // Fulfilled if we have already upgraded to the desired level
    if (BWAPI::Broodwar->self()->getUpgradeLevel(type) >= level) return true;

    // Fulfilled if something is currently upgrading it
    // Technically we still may want to go further, but we don't need to plan that in advance
    for (auto unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (!unit->isCompleted()) continue;
        if (unit->getType() != type.whatUpgrades()) continue;

        if (unit->isUpgrading() && unit->getUpgrade() == type) return true;
    }

    return false;
}

BWAPI::UnitType UpgradeProductionGoal::prerequisiteForNextLevel()
{
    if (isFulfilled()) return BWAPI::UnitTypes::None;

    return type.whatsRequired(BWAPI::Broodwar->self()->getUpgradeLevel(type) + 1);
}
