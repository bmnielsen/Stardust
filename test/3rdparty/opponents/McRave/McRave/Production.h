#pragma once
#include <BWAPI.h>

namespace McRave::Production
{
    int getReservedMineral();
    int getReservedGas();
    double scoreUnit(BWAPI::UnitType);
    bool hasIdleProduction();
    void onFrame();

    bool larvaTrickRequired(UnitInfo& unit);
    bool larvaTrickOptional(UnitInfo& unit);
}