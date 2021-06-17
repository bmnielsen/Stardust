#include "UnitCluster.h"

#include "Map.h"
#include "Players.h"
#include "Geo.h"
#include "UnitUtil.h"

void UnitCluster::flee(std::set<Unit> &enemyUnits)
{
    // Form an arc if none of our units are in danger
    // Currently disabled as we can't really do this safely
    move(Map::getMyMain()->getPosition());
    return;

    auto &grid = Players::grid(BWAPI::Broodwar->enemy());
    bool canFormArc = true;
    for (auto &unit : units)
    {
        if ((unit->isFlying
             ? grid.airThreat(unit->lastPosition)
             : grid.groundThreat(unit->lastPosition)) > 0)
        {
            canFormArc = false;
            break;
        }
    }
    if (!canFormArc)
    {
        move(Map::getMyMain()->getPosition());
        return;
    }

    // Get the nearest enemy combat unit to our vanguard
    Unit nearestEnemy = nullptr;
    int nearestEnemyDist = INT_MAX;
    for (auto &unit : enemyUnits)
    {
        if (!unit->canAttack(vanguard)) continue;
        int dist = vanguard->lastPosition.getApproxDistance(unit->lastPosition);
        if (dist < nearestEnemyDist)
        {
            nearestEnemyDist = dist;
            nearestEnemy = unit;
        }
    }
    if (!nearestEnemy)
    {
        move(Map::getMyMain()->getPosition());
        return;
    }

    auto pivot = vanguard->lastPosition + Geo::ScaleVector(nearestEnemy->lastPosition - vanguard->lastPosition,
                                                           vanguard->lastPosition.getApproxDistance(nearestEnemy->lastPosition) + 64);

    // Determine the desired distance to the pivot
    // If our units are on average within one tile of where we want them, increase the distance by one tile
    // Otherwise wait until they have formed up around the vanguard

    auto effectiveDist = [&pivot](const MyUnit &unit)
    {
        return unit->lastPosition.getApproxDistance(pivot) + (UnitUtil::IsRangedUnit(unit->type) ? 0 : 32);
    };

    int desiredDistance = effectiveDist(vanguard);

    int accumulator = 0;
    int count = 0;
    int countWithinLimit = 0;
    for (auto &unit : units)
    {
        if (unit->isFlying) continue;

        int dist = effectiveDist(unit);
        if (dist <= (desiredDistance + 24)) countWithinLimit++;
        if (countWithinLimit >= 8) break;
        accumulator += dist;
        count++;
    }

    if (countWithinLimit >= 8 || (accumulator / count) <= (desiredDistance + 24)) desiredDistance += 32;

    if (formArc(pivot, desiredDistance)) return;

    move(Map::getMyMain()->getPosition());
}
