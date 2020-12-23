#include "ProductionGoal.h"

BWAPI::UnitType UpgradeProductionGoal::prerequisiteForNextLevel() const
{
    if (type.isTechType()) return BWAPI::UnitTypes::None;

    return type.upgradeType.whatsRequired(BWAPI::Broodwar->self()->getUpgradeLevel(type.upgradeType) + 1);
}
