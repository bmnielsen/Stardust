#include "ProductionGoal.h"

BWAPI::UnitType UpgradeProductionGoal::prerequisiteForNextLevel() const
{
    return type.whatsRequired(BWAPI::Broodwar->self()->getUpgradeLevel(type) + 1);
}
