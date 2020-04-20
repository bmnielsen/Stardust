#pragma once

#include "StrategyEngine.h"

class PvT : public StrategyEngine
{
public:
    void initialize(std::vector<std::shared_ptr<Play>> &plays) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;
};
