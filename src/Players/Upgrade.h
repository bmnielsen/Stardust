#pragma once

#include "ProductionGoal.h"

class Upgrade : public UpgradeProductionGoal
{
public:
    Upgrade(BWAPI::UpgradeType type, int level = 1) : type(type), level(level) {}

    BWAPI::UpgradeType upgradeType() { return type; }
    bool isFulfilled();

private:
    BWAPI::UpgradeType type;
    int                level;
};