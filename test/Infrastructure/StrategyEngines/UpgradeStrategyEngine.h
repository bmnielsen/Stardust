#pragma once

#include "StrategyEngine.h"
#include "Units.h"

class UpgradeStrategyEngine : public StrategyEngine
{
public:
    BWAPI::UpgradeType upgrade;
    int level;

    UpgradeStrategyEngine(BWAPI::UpgradeType upgrade, int level = 1) : upgrade(upgrade), level(level) {}

protected:
    void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override
    {
        if (BWAPI::Broodwar->self()->getUpgradeLevel(upgrade) >= level) return;
        if (Units::isBeingUpgraded(upgrade)) return;

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(UpgradeProductionGoal(upgrade));
    }
};
