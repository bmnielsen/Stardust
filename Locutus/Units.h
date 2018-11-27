#pragma once

#include "Common.h"

#include "MyUnit.h"
#include "EnemyUnit.h"

namespace Units
{
    void update();
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitRenegade(BWAPI::Unit unit);

    const std::map<BWAPI::Unit, std::shared_ptr<MyUnit>> & allMine();
    std::shared_ptr<MyUnit> getMine(BWAPI::Unit unit);

    const std::map<BWAPI::Unit, std::shared_ptr<EnemyUnit>> & allEnemy();
    std::shared_ptr<EnemyUnit> getEnemy(BWAPI::Unit unit);

    int countAll(BWAPI::UnitType type);
    int countCompleted(BWAPI::UnitType type);
    int countIncomplete(BWAPI::UnitType type);
}
