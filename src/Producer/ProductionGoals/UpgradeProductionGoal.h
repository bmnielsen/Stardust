#pragma once

#include "Common.h"

class UpgradeProductionGoal
{
public:
    UpgradeProductionGoal(BWAPI::UpgradeType type, int level = 1) : type(type), level(level) {}

    // The upgrade type
    BWAPI::UpgradeType upgradeType() { return type; }

    // Whether or not the production goal is currently fulfilled
    bool isFulfilled();

    // Prerequisite for the next upgrade level
    BWAPI::UnitType prerequisiteForNextLevel();

private:
    BWAPI::UpgradeType type;
    int                level;
};
