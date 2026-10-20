#include "DefendWallSquad.h"

#include "BuildingPlacement.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"

DefendWallSquad::DefendWallSquad()
        : UnitSquad("Defend wall")
{
    wall = BuildingPlacement::getForgeGatewayWall();
    if (!wall.isValid())
    {
        AppLog::Get() << "ERROR: DefendWallSquad for invalid wall";
    }

    targetPosition = wall.gapCenter;
}

void DefendWallSquad::execute(UnitCluster &clump)
{
    auto combatUnitSeenRecentlyPredicate = [](const Unit &gameUnit)
    {
        if (!gameUnit->type.isBuilding() && gameUnit->lastSeen < (frameNow - 48)) return false;
        if (!UnitHelper::IsCombatUnit(gameUnit->type) && gameUnit->lastSeenAttacking < (frameNow - 120)) return false;
        if (!gameUnit->isTransport() && !UnitHelper::CanAttackGround(gameUnit->type)) return false;

        return true;
    };

    /* Gather all enemy units in our main or natural area or close to the wall center */
    std::set<Unit> enemyUnits;
    UnitMgr::enemyInRadius(enemyUnits, targetPosition, 240, combatUnitSeenRecentlyPredicate);
    for (const auto &area : Map::getMyMainAreas()) UnitMgr::enemyInArea(enemyUnits, area, combatUnitSeenRecentlyPredicate);

    auto natBase = Map::mapSpecificOverride()->naturalForWallPlacement(Map::getMyMain());
    if (!natBase) natBase = Map::getMyNatural();
    if (natBase) UnitMgr::enemyInArea(enemyUnits, natBase->getArea(), combatUnitSeenRecentlyPredicate);

    /* Remove enemy units that are outside and not close to the wall */
    for (auto iterRef = enemyUnits.begin(); iterRef != enemyUnits.end(); )
    {
        auto tilePos = (*iterRef)->getTilePosition();
        if (wall.tilesOutsideWall.contains(tilePos) && !wall.tilesOutsideButCloseToWall.contains(tilePos))
        {
            iterRef = enemyUnits.erase(iterRef);
        }
        else
        {
            iterRef++;
        }
    }

    /* If there are no enemy units, move to the wall center */
    if (enemyUnits.empty())
    {
        clump.setActivity(UnitCluster::Activity::Moving);

        /* If any of our units are outside the wall, move to our main to give them room to get in */
        bool unitOutsideWall = false;
        for (const auto &gameUnit : clump.units)
        {
            if (!wall.tilesOutsideWall.contains(gameUnit->getTilePosition())) continue;
            unitOutsideWall = true;
            break;
        }

        if (unitOutsideWall)
        {
            clump.move(Map::getMyMain()->getPosition());
        }
        else
        {
            clump.move(targetPosition);
        }

        return;
    }

    auto unitsAndTargets = clump.selectTargets(enemyUnits, targetPosition);
    clump.setActivity(UnitCluster::Activity::Attacking);
    clump.attack(unitsAndTargets, targetPosition);
}
