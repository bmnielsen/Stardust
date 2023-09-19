#pragma once

#include "StrategyEngine.h"

class DoNothingStrategyEngine : public StrategyEngine
{
    void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom, const std::string &openingOverride) override {}

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override
    {}
};
