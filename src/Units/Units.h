#pragma once

#include "Common.h"

#include "MyUnit.h"
#include "Unit.h"

namespace Units
{
    void update();
    void onUnitDestroy(BWAPI::Unit unit);
    void onBulletCreate(BWAPI::Bullet bullet);

    Unit const& get(BWAPI::Unit unit);
    std::set<std::shared_ptr<Unit>> & getForPlayer(BWAPI::Player player);

    void getInRadius(std::set<std::shared_ptr<Unit>> &units, BWAPI::Player player, BWAPI::Position position, int radius);
    void getInArea(std::set<std::shared_ptr<Unit>> &units, BWAPI::Player player, const BWEM::Area * area, std::function<bool(const std::shared_ptr<Unit>&)> predicate = nullptr);

    MyUnit& getMine(BWAPI::Unit unit);
    int countAll(BWAPI::UnitType type);
    int countCompleted(BWAPI::UnitType type);
    int countIncomplete(BWAPI::UnitType type);
    std::map<BWAPI::UnitType, int> countIncompleteByType();
}
