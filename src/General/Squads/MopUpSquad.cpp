#include "MopUpSquad.h"

#include "Units.h"
#include "PathFinding.h"
#include "Map.h"

#if INSTRUMENTATION_ENABLED
#define DEBUG_SQUAD_TARGET true
#endif

MopUpSquad::MopUpSquad() : UnitSquad("Mop Up")
{
    targetPosition = Map::getMyMain()->getPosition();
}

void MopUpSquad::execute(UnitCluster &clump)
{
    /* If there are enemy units near the cluster, attack them */
    /* TODO: Refactor so we can use the same code as in AttackBaseSquad (combat sim, etc.) */
    std::set<Unit> enemyUnits;
    UnitMgr::enemyInRadius(enemyUnits, clump.vanguard->prevPos, 750);
    if (!enemyUnits.empty())
    {
        updateDetectionNeeds(enemyUnits);

        auto unitsAndTargets = clump.selectTargets(enemyUnits, targetPosition);

        /* If any of our units has a target, attack */
        bool hasTarget = false;
        for (const auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.second)
            {
                hasTarget = true;
                break;
            }
        }
        if (hasTarget)
        {
#if DEBUG_SQUAD_TARGET
            Telemetry::log() << "MopUp cluster " << BWAPI::WalkPosition(clump.midpoint) << ": attacking enemy units near cluster";
            Telemetry::log() << "First unit: " << **enemyUnits.begin();
#endif

            clump.setActivity(UnitCluster::Activity::Attacking);
            clump.attack(unitsAndTargets, targetPosition);
            return;
        }
    }

    /* Search for the closest known enemy building to the cluster */
    int closestDist = INT_MAX;
    BWAPI::Position closestPosition = BWAPI::Positions::Invalid;
    for (auto &enemyUnit : UnitMgr::allEnemy())
    {
        if (!enemyUnit->type.isBuilding()) continue;
        if (!enemyUnit->lastPositionValid || !enemyUnit->prevPos.isValid()) continue;

        int span = enemyUnit->isFlying
                   ? enemyUnit->prevPos.getApproxDistance(clump.vanguard->prevPos)
                   : Pathfinder::GetGroundDistance(clump.vanguard->prevPos, enemyUnit->prevPos);
        if (span != -1 && span < closestDist)
        {
            closestDist = span;
            closestPosition = enemyUnit->prevPos;
        }
    }

    /* If we found one, move towards it */
    if (closestPosition.isValid())
    {
#if DEBUG_SQUAD_TARGET
        Telemetry::log() << "MopUp cluster " << BWAPI::WalkPosition(clump.midpoint)
                         << ": attacking known building @ " << BWAPI::WalkPosition(closestPosition);
#endif

        /* Move to the target, will switch to attack when the vanguard is close enough */
        clump.setActivity(UnitCluster::Activity::Moving);

        auto baseLoc = Map::baseNear(closestPosition);
        targetPosition = baseLoc ? baseLoc->getPosition() : closestPosition;
        clump.move(targetPosition);
        return;
    }

    /* If we don't know the enemy's starting main, try to find it first */
    if (!Map::getEnemyStartingMain())
    {
        auto unscoutedStartingLocations = Map::unscoutedStartingLocations();
        if (unscoutedStartingLocations.empty())
        {
#if DEBUG_SQUAD_TARGET
            Telemetry::log() << "MopUp cluster " << BWAPI::WalkPosition(clump.midpoint)
                             << ": don't know enemy starting base but have no unscouted starting locations";
#endif
        }
        else
        {
            auto baseLoc = *unscoutedStartingLocations.begin();

#if DEBUG_SQUAD_TARGET
            Telemetry::log() << "MopUp cluster " << BWAPI::WalkPosition(clump.midpoint)
                             << ": moving to next unscouted starting location @ " << BWAPI::WalkPosition(baseLoc->getPosition());
#endif

            clump.setActivity(UnitCluster::Activity::Moving);
            targetPosition = baseLoc->getPosition();
            clump.move(targetPosition);
            return;
        }
    }

    /* We don't know of any enemy buildings, so try to find one */
    /* TODO: Somehow handle terran floated buildings */
    int bestBaseScoutedAt = frameNow;
    Base *bestBase = nullptr;
    for (auto &baseLoc : Map::getUntakenExpansions(BWAPI::Broodwar->enemy()))
    {
        /* If the base has been scouted in the past few minutes, skip it */
        if (baseLoc->lastScouted > (frameNow - 5000)) continue;

        if (baseLoc->lastScouted < bestBaseScoutedAt)
        {
            bestBaseScoutedAt = baseLoc->lastScouted;
            bestBase = baseLoc;
        }
    }

    if (bestBase)
    {
#if DEBUG_SQUAD_TARGET
        Telemetry::log() << "MopUp cluster " << BWAPI::WalkPosition(clump.midpoint)
                         << ": moving to next base @ " << BWAPI::WalkPosition(bestBase->getPosition());
#endif

        clump.setActivity(UnitCluster::Activity::Moving);
        targetPosition = bestBase->getPosition();
        clump.move(targetPosition);
        return;
    }

    /* Move towards the accessible tile that we have seen longest ago */
    auto grid = Pathfinder::getNavigationGrid(Map::getMyMain()->getPosition(), true);
    if (grid)
    {
        BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;
        int bestFrame = INT_MAX;
        int bestDist = INT_MAX;
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                /* Consider frame seen in "buckets" of 1000 */
                int tick = Map::lastSeen(x, y) / 1000;
                if (tick >= bestFrame) continue;

                BWAPI::TilePosition tilePos(x, y);

                if ((*grid)[tilePos].cost == USHRT_MAX) continue;

                int span = Pathfinder::GetGroundDistance(clump.midpoint, BWAPI::Position(tilePos));
                if (tick < bestFrame || span < bestDist)
                {
                    bestTile = tilePos;
                    bestFrame = tick;
                    bestDist = span;
                }
            }
        }

        if (bestTile.isValid())
        {
#if DEBUG_SQUAD_TARGET
            Telemetry::log() << "MopUp cluster " << BWAPI::WalkPosition(clump.midpoint)
                             << ": moving to best tile @ " << BWAPI::WalkPosition(bestTile)
                             << "; frame seen: " << bestFrame;
#endif

            clump.setActivity(UnitCluster::Activity::Moving);
            targetPosition = BWAPI::Position(bestTile) + BWAPI::Position(16, 16);
            clump.move(targetPosition);
            return;
        }
    }

#if DEBUG_SQUAD_TARGET
    Telemetry::log() << "MopUp cluster " << BWAPI::WalkPosition(clump.midpoint) << ": Nothing to do!";
#endif
    targetPosition = Map::getMyMain()->getPosition();
}
