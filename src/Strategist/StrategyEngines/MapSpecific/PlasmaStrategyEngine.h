#pragma once

#include "StrategyEngine.h"

class PlasmaStrategyEngine : public StrategyEngine
{
public:
    void initialize(std::vector<std::shared_ptr<Play>> &plays) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

private:
    static void handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                       std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);
};
