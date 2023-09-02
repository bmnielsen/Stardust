#include "DefendWallSquad.h"

#include "BuildingPlacement.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"

DefendWallSquad::DefendWallSquad()
        : Squad("Defend wall")
{
    wall = BuildingPlacement::getForgeGatewayWall();
    if (!wall.isValid())
    {
        Log::Get() << "ERROR: DefendWallSquad for invalid wall";
    }

    targetPosition = wall.gapCenter;
}

void DefendWallSquad::execute(UnitCluster &cluster)
{
    auto combatUnitSeenRecentlyPredicate = [](const Unit &unit)
    {
        if (!unit->type.isBuilding() && unit->lastSeen < (currentFrame - 48)) return false;
        if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (currentFrame - 120)) return false;
        if (!unit->isTransport() && !UnitUtil::CanAttackGround(unit->type)) return false;

        return true;
    };

    // Gather all enemy units in our main or natural area or close to the wall center
    std::set<Unit> enemyUnits;
    Units::enemyInRadius(enemyUnits, targetPosition, 240, combatUnitSeenRecentlyPredicate);
    for (const auto &area : Map::getMyMainAreas()) Units::enemyInArea(enemyUnits, area, combatUnitSeenRecentlyPredicate);
    Units::enemyInArea(enemyUnits, Map::getMyNatural()->getArea(), combatUnitSeenRecentlyPredicate);

    // Remove enemy units that are outside and not close to the wall
    for (auto it = enemyUnits.begin(); it != enemyUnits.end(); )
    {
        auto tile = (*it)->getTilePosition();
        if (wall.tilesOutsideWall.contains(tile) && !wall.tilesOutsideButCloseToWall.contains(tile))
        {
            it = enemyUnits.erase(it);
        }
        else
        {
            it++;
        }
    }

    // If there are no enemy units, move to the wall center
    if (enemyUnits.empty())
    {
        cluster.setActivity(UnitCluster::Activity::Moving);

        // If any of our units are outside the wall, move to our main to give them room to get in
        bool unitOutsideWall = false;
        for (const auto &unit : cluster.units)
        {
            if (!wall.tilesOutsideWall.contains(unit->getTilePosition())) continue;
            unitOutsideWall = true;
            break;
        }

        if (unitOutsideWall)
        {
            cluster.move(Map::getMyMain()->getPosition());
        }
        else
        {
            cluster.move(targetPosition);
        }

        return;
    }

    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);
    cluster.setActivity(UnitCluster::Activity::Attacking);
    cluster.attack(unitsAndTargets, targetPosition);
}
