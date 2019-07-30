#pragma once

#include "Common.h"
#include "ProductionGoals/UnitProductionGoal.h"
#include "ProductionGoals/UpgradeProductionGoal.h"
#include <variant>

using ProductionGoal = std::variant<UnitProductionGoal, UpgradeProductionGoal>;
