#pragma once

#include "Common.h"

#include "ProductionGoal.h"
#include "Play.h"

namespace Strategist
{
    void update();

    void chooseOpening();

    void setOpening(std::vector<std::shared_ptr<Play>> openingPlays);

    std::vector<ProductionGoal> &currentProductionGoals();
}
