#pragma once

#include "Common.h"

namespace Workers
{
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitRenegade(BWAPI::Unit unit);
    void update();
}
