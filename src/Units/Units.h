#pragma once

#include "Common.h"

#include "Resource.h"
#include "MyUnit.h"
#include "Unit.h"
#include "UpgradeOrTechType.h"
#include "Base.h"

namespace UnitMgr
{
    void initialize();

    void update();

    void issueOrders();

    void onUnitDestroy(BWAPI::Unit gameUnit);

    void onBulletCreate(BWAPI::Bullet bullet);

    Unit get(BWAPI::Unit gameUnit);

    OwnedUnit mine(BWAPI::Unit gameUnit);

    OwnedUnit myBuildingAt(BWAPI::TilePosition tilePos);

    std::unordered_set<OwnedUnit> &allMine();

    std::unordered_set<OwnedUnit> &allMineCompletedOfType(BWAPI::UnitType type);

    std::unordered_set<OwnedUnit> &allMineIncompleteOfType(BWAPI::UnitType type);

    std::map<BWAPI::UnitType, std::unordered_set<OwnedUnit>> &allMineIncompleteByType();

    std::unordered_set<Unit> &allEnemy();

    std::unordered_set<Unit> &allEnemyOfType(BWAPI::UnitType type);

    void mine(std::set<OwnedUnit> &units,
              const std::function<bool(const OwnedUnit &)> &predicate = nullptr);

    void enemy(std::set<Unit> &units,
               const std::function<bool(const Unit &)> &predicate = nullptr);

    void enemyInRadius(std::set<Unit> &units,
                       BWAPI::Position position,
                       int radius,
                       const std::function<bool(const Unit &)> &predicate = nullptr);

    void enemyInArea(std::set<Unit> &units,
                     const BWEM::Area *area,
                     const std::function<bool(const Unit &)> &predicate = nullptr);

    std::unordered_set<Unit> &enemyAtBase(Base *baseLoc);

    int countAll(BWAPI::UnitType type);

    int countCompleted(BWAPI::UnitType type);

    int countIncomplete(BWAPI::UnitType type);

    std::map<BWAPI::UnitType, int> countIncompleteByType();

    int countEnemy(BWAPI::UnitType type);

    std::vector<std::pair<int, int>> &getEnemyUnitTimings(BWAPI::UnitType type);

    bool hasEnemyBuilt(BWAPI::UnitType type);

    Resource resourceAt(BWAPI::TilePosition tilePos);

    std::vector<Resource> myCompletedRefineries();

    /* TODO: This doesn't really fit here */
    bool isBeingUpgradedOrResearched(UpgradeOrTechType type);

#if LOGGING_ENABLED
    void setLogUnitsCreatedAndLost(bool newValue);
#endif
}
