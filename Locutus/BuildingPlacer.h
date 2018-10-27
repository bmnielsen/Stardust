#pragma once

#include "Common.h"

namespace BuildingPlacer
{
    void initialize();
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitMorph(BWAPI::Unit unit);
    void onUnitCreate(BWAPI::Unit unit);
    void onUnitDiscover(BWAPI::Unit unit);
}