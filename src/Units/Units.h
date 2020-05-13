#pragma once

#include "Common.h"

#include "MyUnit.h"
#include "Unit.h"

namespace Units
{
    void initialize();

    void update();

    void issueOrders();

    void onUnitDestroy(BWAPI::Unit unit);

    void onBulletCreate(BWAPI::Bullet bullet);

    Unit get(BWAPI::Unit unit);

    MyUnit mine(BWAPI::Unit unit);

    MyUnit myBuildingAt(BWAPI::TilePosition tile);

    std::set<MyUnit> &allMine();

    std::set<Unit> &allEnemy();

    void mine(std::set<MyUnit> &units,
              const std::function<bool(const MyUnit &)> &predicate = nullptr);

    void enemy(std::set<Unit> &units,
               const std::function<bool(const Unit &)> &predicate = nullptr);

    void enemyInRadius(std::set<Unit> &units,
                       BWAPI::Position position,
                       int radius,
                       const std::function<bool(const Unit &)> &predicate = nullptr);

    void enemyInArea(std::set<Unit> &units,
                     const BWEM::Area *area,
                     const std::function<bool(const Unit &)> &predicate = nullptr);

    int countAll(BWAPI::UnitType type);

    int countCompleted(BWAPI::UnitType type);

    int countIncomplete(BWAPI::UnitType type);

    std::map<BWAPI::UnitType, int> countIncompleteByType();

    int countEnemy(BWAPI::UnitType type);

    std::vector<std::pair<int, int>> &getEnemyUnitTimings(BWAPI::UnitType type);

    bool hasEnemyBuilt(BWAPI::UnitType type);

    // TODO: This doesn't really fit here
    bool isBeingUpgraded(BWAPI::UpgradeType type);
}
