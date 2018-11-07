#pragma once

#include "Common.h"

#include "MyUnit.h"
#include "EnemyUnit.h"

namespace Units
{
    void update();
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitRenegade(BWAPI::Unit unit);

    const std::map<BWAPI::Unit, MyUnit> & allMine();
    MyUnit & getMine(BWAPI::Unit unit);

    const std::map<BWAPI::Unit, EnemyUnit> & allEnemy();
    EnemyUnit & getEnemy(BWAPI::Unit unit);
}
