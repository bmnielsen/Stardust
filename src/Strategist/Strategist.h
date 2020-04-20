#pragma once

#include "Common.h"

#include "ProductionGoal.h"
#include "Play.h"
#include "StrategyEngine.h"

namespace Strategist
{
    void update();

    void initialize();

    std::vector<ProductionGoal> &currentProductionGoals();

    std::vector<std::pair<int, int>> &currentMineralReservations();

    void setOpening(std::vector<std::shared_ptr<Play>> openingPlays);

    void setStrategyEngine(std::unique_ptr<StrategyEngine> strategyEngine);
}
