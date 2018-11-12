#pragma once

#include "Common.h"

namespace Builder
{
    void update();

    BWAPI::Unit getBuilderUnit(BWAPI::TilePosition tile, BWAPI::UnitType type, int * expectedArrivalFrame = nullptr);
}
