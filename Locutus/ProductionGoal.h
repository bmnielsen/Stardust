#pragma once

#include "Common.h"
#include "UnitProductionGoal.h"
#include "UpgradeProductionGoal.h"
#include <variant>

using ProductionGoal = std::variant<UnitProductionGoal, UpgradeProductionGoal>;
