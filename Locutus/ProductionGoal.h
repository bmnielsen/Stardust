#pragma once

#include "Common.h"

class ProductionGoal
{
public:
    enum Type { ContinuousProduction };

    Type            type;       // The type of production goal
    BWAPI::UnitType unitType;   // The type of unit to produce
    int             count;      // How many of the unit to produce

    ProductionGoal(Type type, BWAPI::UnitType unitType, int count = -1);
};
