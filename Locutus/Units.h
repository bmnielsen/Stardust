#pragma once

#include "Common.h"

#include "MyUnit.h"
#include "EnemyUnit.h"

namespace Units
{
    void update();
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitRenegade(BWAPI::Unit unit);

    MyUnit& getMine(BWAPI::Unit unit);
    EnemyUnit& getEnemy(BWAPI::Unit unit);

    int countAll(BWAPI::UnitType type);
    int countCompleted(BWAPI::UnitType type);
    int countIncomplete(BWAPI::UnitType type);
}
