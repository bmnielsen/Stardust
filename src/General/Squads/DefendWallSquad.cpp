#include "DefendWallSquad.h"

#include "BuildingPlacement.h"
#include "Units.h"
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
    // Gather all enemy units in our main or natural area or close to the wall center
    std::set<Unit> enemyUnits;
    Units::enemyInRadius(enemyUnits, targetPosition, 240);
    for (const auto &area : Map::getMyMainAreas()) Units::enemyInArea(enemyUnits, area);
    Units::enemyInArea(enemyUnits, Map::getMyNatural()->getArea());

    // Remove enemy units that are outside the wall
    for (auto it = enemyUnits.begin(); it != enemyUnits.end(); )
    {
        auto tile = (*it)->getTilePosition();
        if (wall.tilesOutsideWall.find(tile) != wall.tilesOutsideWall.end() &&
            wall.tilesOutsideButCloseToWall.find(tile) == wall.tilesOutsideButCloseToWall.end())
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
            if (wall.tilesOutsideWall.find(unit->getTilePosition()) == wall.tilesOutsideWall.end()) continue;
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
