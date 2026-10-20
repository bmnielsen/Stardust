#include "UnitCluster.h"

#include "Map.h"
#include "Players.h"
#include "Geo.h"
#include "UnitUtil.h"

void UnitCluster::flee(std::set<Unit> &enemyUnits)
{
    /* Form an arc if none of our units are in danger */
    /* Currently disabled as we can't really do this safely */
    move(Map::getMyMain()->getPosition());
    return;

    auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());
    bool canFormArc = true;
    for (auto &gameUnit : units)
    {
        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;

        if ((gameUnit->isFlying
             ? grid.airThreat(gameUnit->prevPos)
             : grid.groundThreat(gameUnit->prevPos)) > 0)
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

    /* Get the nearest enemy combat unit to our vanguard */
    Unit nearestEnemy = nullptr;
    int nearestEnemyDist = INT_MAX;
    for (auto &gameUnit : enemyUnits)
    {
        if (!gameUnit->canAttack(vanguard)) continue;
        int span = vanguard->prevPos.getApproxDistance(gameUnit->prevPos);
        if (span < nearestEnemyDist)
        {
            nearestEnemyDist = span;
            nearestEnemy = gameUnit;
        }
    }
    if (!nearestEnemy)
    {
        move(Map::getMyMain()->getPosition());
        return;
    }

    auto pivot = vanguard->prevPos + Geom::ScaleVector(nearestEnemy->prevPos - vanguard->prevPos,
                                                           vanguard->prevPos.getApproxDistance(nearestEnemy->prevPos) + 64);

    /* Determine the desired distance to the pivot */
    /* If our units are on average within one tile of where we want them, increase the distance by one tile */
    /* Otherwise wait until they have formed up around the vanguard */

    auto effectiveDist = [&pivot](const OwnedUnit &gameUnit)
    {
        return gameUnit->prevPos.getApproxDistance(pivot) + (UnitHelper::IsRangedUnit(gameUnit->type) ? 0 : 32);
    };

    int desiredDistance = effectiveDist(vanguard);

    int accumulator = 0;
    int count = 0;
    int countWithinLimit = 0;
    for (auto &gameUnit : units)
    {
        if (gameUnit->isFlying) continue;
        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;

        int span = effectiveDist(gameUnit);
        if (span <= (desiredDistance + 24)) countWithinLimit++;
        if (countWithinLimit >= 8) break;
        accumulator += span;
        count++;
    }

    if (countWithinLimit >= 8 || (accumulator / count) <= (desiredDistance + 24)) desiredDistance += 32;

    if (formArc(pivot, desiredDistance)) return;

    move(Map::getMyMain()->getPosition());
}
