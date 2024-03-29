#pragma once

#include <variant>

#include "StrategyEngines/PvP.h"
#include "StrategyEngines/PvT.h"
#include "StrategyEngines/PvZ.h"
#include "StrategyEngines/PvU.h"

namespace Strategist
{
    bool isOurStrategy(std::variant<PvP::OurStrategy, PvT::OurStrategy, PvZ::OurStrategy, PvU::OurStrategy> strategy);

    bool isEnemyStrategy(std::variant<PvP::ProtossStrategy, PvT::TerranStrategy, PvZ::ZergStrategy> strategy);
}
