#pragma once

#include "Common.h"

#include "MyUnit.h"
#include "Unit.h"

namespace Units
{
    void update();
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitRenegade(BWAPI::Unit unit);
    void onBulletCreate(BWAPI::Bullet bullet);

    Unit const& get(BWAPI::Unit unit);
    std::set<std::shared_ptr<Unit>> & getForPlayer(BWAPI::Player player);

    std::set<std::shared_ptr<Unit>> getInRadius(BWAPI::Player player, BWAPI::Position position, int radius);

    MyUnit& getMine(BWAPI::Unit unit);
    int countAll(BWAPI::UnitType type);
    int countCompleted(BWAPI::UnitType type);
    int countIncomplete(BWAPI::UnitType type);
}
