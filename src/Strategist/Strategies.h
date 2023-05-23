#pragma once

#include <variant>

#include "StrategyEngines/PvP.h"
#include "StrategyEngines/PvT.h"
#include "StrategyEngines/PvZ.h"

namespace Strategist
{
    bool isEnemyStrategy(std::variant<PvP::ProtossStrategy, PvT::TerranStrategy, PvZ::ZergStrategy> strategy);
}
