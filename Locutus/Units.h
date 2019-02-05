#pragma once

#include "Common.h"

#include "MyUnit.h"
#include "Unit.h"

namespace Units
{
    void update();
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitRenegade(BWAPI::Unit unit);

    std::set<std::shared_ptr<Unit>> & getForPlayer(BWAPI::Player player);
    Unit& get(BWAPI::Unit unit);

    MyUnit& getMine(BWAPI::Unit unit);
    int countAll(BWAPI::UnitType type);
    int countCompleted(BWAPI::UnitType type);
    int countIncomplete(BWAPI::UnitType type);
}
