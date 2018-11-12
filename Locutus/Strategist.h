#pragma once

#include "Common.h"

#include "ProductionGoal.h"

namespace Strategist
{
    void chooseOpening();
    std::vector<ProductionGoal> & currentProductionGoals();
}
