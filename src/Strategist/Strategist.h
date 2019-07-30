#pragma once

#include "Common.h"

#include "ProductionGoal.h"

namespace Strategist
{
    void update();

    void chooseOpening();

    std::vector<ProductionGoal> &currentProductionGoals();
}
