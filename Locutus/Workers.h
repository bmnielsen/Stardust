#pragma once

#include "Common.h"

namespace Workers
{
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitRenegade(BWAPI::Unit unit);
    void update();

    // Whether the given worker unit can currently be reassigned to build something
    bool isAvailableBuilder(BWAPI::Unit unit);

    void setBuilder(BWAPI::Unit unit);
    
    void releaseWorker(BWAPI::Unit unit);
}
