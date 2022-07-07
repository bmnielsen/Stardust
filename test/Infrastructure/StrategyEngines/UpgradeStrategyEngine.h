#pragma once

#include "StrategyEngine.h"
#include "Units.h"

class UpgradeStrategyEngine : public StrategyEngine
{
public:
    UpgradeOrTechType upgradeOrTechType;
    int level;

    UpgradeStrategyEngine(UpgradeOrTechType upgradeOrTechType, int level = 1) : upgradeOrTechType(upgradeOrTechType), level(level) {}

protected:
    void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override
    {
        if (upgradeOrTechType.currentLevel() > 0) return;
        if (Units::isBeingUpgradedOrResearched(upgradeOrTechType)) return;

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(UpgradeProductionGoal("test", upgradeOrTechType));
    }
};
