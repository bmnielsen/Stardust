#include "UnitCluster.h"

#include "Map.h"
#include "Geo.h"

/*
 * Stands ground out of range of the enemy.
 *
 * Usually attempts to form an arc, but will either pull back or group up around the army center if this isn't possible.
 */

void UnitCluster::standGround(std::set<Unit> &enemyUnits, BWAPI::Position targetPosition)
{
    /* Get the pivot point */
    /* Normally the closest enemy ground unit to the vanguard */
    auto pivot = BWAPI::Positions::Invalid;
    int closestDist = INT_MAX;
    for (const auto &gameUnit : enemyUnits)
    {
        if (gameUnit->isFlying) continue;
        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;
        if (!gameUnit->lastPositionValid) continue;

        int span = vanguard->getDistance(gameUnit);
        if (span < closestDist)
        {
            closestDist = span;
            pivot = gameUnit->prevPos;
        }
    }

    /* If there was no enemy unit to use, pick a spot ahead of the vanguard towards the target position */
    if (pivot == BWAPI::Positions::Invalid)
    {
        auto waypoint = Pathfinder::NextGridOrChokeWaypoint(vanguard->prevPos,
                targetPosition,
                Pathfinder::getNavigationGrid(targetPosition),
                3,
                true);
        if (waypoint != BWAPI::Positions::Invalid)
        {
            pivot = vanguard->prevPos + Geom::ScaleVector(waypoint - vanguard->prevPos, 8 * 32);
        }
    }

    /* Attempt to form the arc */
    if (pivot.isValid() && formArc(pivot, std::min(15 * 32, vanguard->prevPos.getApproxDistance(pivot)))) return;

    /* We couldn't form an arc here */
    /* If the center of the cluster is walkable, move towards it */
    /* Otherwise move towards the vanguard with the assumption that the center will become walkable soon */
    /* (otherwise this results in forward motion as units move ahead and become the new vanguard) */
    if (Map::isWalkable(BWAPI::TilePosition(midpoint)))
    {
        move(midpoint);
    }
    else
    {
        move(vanguard->prevPos);
    }
}
