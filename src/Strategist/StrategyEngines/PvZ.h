#pragma once

#include "StrategyEngine.h"

class PvZ : public StrategyEngine
{
public:
    void initialize(std::vector<std::shared_ptr<Play>> &plays) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

private:
    static void handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);
};
