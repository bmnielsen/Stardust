#include "MopUpSquad.h"

#include "Units.h"
#include "PathFinding.h"
#include "Map.h"

#if INSTRUMENTATION_ENABLED
#define DEBUG_SQUAD_TARGET true
#endif

MopUpSquad::MopUpSquad() : Squad("Mop Up")
{
    targetPosition = Map::getMyMain()->getPosition();
}

void MopUpSquad::execute(UnitCluster &cluster)
{
    // If there are enemy units near the cluster, attack them
    // TODO: Refactor so we can use the same code as in AttackBaseSquad (combat sim, etc.)
    std::set<Unit> enemyUnits;
    Units::enemyInRadius(enemyUnits, cluster.center, 480);
    if (!enemyUnits.empty())
    {
        updateDetectionNeeds(enemyUnits);

        auto unitsAndTargets = cluster.selectTargets(enemyUnits, cluster.center);

        // If any of our units has a target, attack
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
            CherryVis::log() << "MopUp cluster " << BWAPI::WalkPosition(cluster.center) << ": attacking enemy units near cluster";
            CherryVis::log() << "First unit: " << **enemyUnits.begin();
#endif

            cluster.attack(unitsAndTargets, cluster.center);
            return;
        }
    }

    // Search for the closest known enemy building to the cluster
    int closestDist = INT_MAX;
    BWAPI::Position closestPosition = BWAPI::Positions::Invalid;
    for (auto &enemyUnit : Units::allEnemy())
    {
        if (!enemyUnit->type.isBuilding()) continue;
        if (!enemyUnit->lastPositionValid || !enemyUnit->lastPosition.isValid()) continue;

        int dist = enemyUnit->isFlying
                   ? enemyUnit->lastPosition.getApproxDistance(cluster.center)
                   : PathFinding::GetGroundDistance(cluster.center, enemyUnit->lastPosition);
        if (dist < closestDist)
        {
            closestDist = dist;
            closestPosition = enemyUnit->lastPosition;
        }
    }

    // If we found one, move towards it
    if (closestPosition.isValid())
    {
#if DEBUG_SQUAD_TARGET
        CherryVis::log() << "MopUp cluster " << BWAPI::WalkPosition(cluster.center)
                         << ": attacking known building @ " << BWAPI::WalkPosition(closestPosition);
#endif

        cluster.setActivity(UnitCluster::Activity::Moving);

        auto base = Map::baseNear(closestPosition);
        cluster.move(base ? base->getPosition() : closestPosition);
        return;
    }

    // We don't know of any enemy buildings, so try to find one
    // TODO: Somehow handle terran floated buildings
    int bestBaseScoutedAt = BWAPI::Broodwar->getFrameCount();
    Base *bestBase = nullptr;
    for (auto &base : Map::getUntakenExpansions(BWAPI::Broodwar->enemy()))
    {
        // If the base has been scouted in the past few minutes, skip it
        if (base->lastScouted > (BWAPI::Broodwar->getFrameCount() - 5000)) continue;

        if (base->lastScouted < bestBaseScoutedAt)
        {
            bestBaseScoutedAt = base->lastScouted;
            bestBase = base;
        }
    }

    if (bestBase)
    {
#if DEBUG_SQUAD_TARGET
        CherryVis::log() << "MopUp cluster " << BWAPI::WalkPosition(cluster.center)
                         << ": moving to next base @ " << BWAPI::WalkPosition(bestBase->getPosition());
#endif

        cluster.setActivity(UnitCluster::Activity::Moving);
        cluster.move(bestBase->getPosition());
        return;
    }

    // Move towards the accessible tile that we have seen longest ago
    auto grid = PathFinding::getNavigationGrid(Map::getMyMain()->getTilePosition(), true);
    if (grid)
    {
        BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;
        int bestFrame = INT_MAX;
        int bestDist = INT_MAX;
        for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
        {
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                // Consider frame seen in "buckets" of 1000
                int frame = Map::lastSeen(x, y) / 1000;
                if (frame >= bestFrame) continue;

                BWAPI::TilePosition tile(x, y);

                if ((*grid)[tile].cost == USHRT_MAX) continue;

                int dist = PathFinding::GetGroundDistance(cluster.center, BWAPI::Position(tile));
                if (frame < bestFrame || dist < bestDist)
                {
                    bestTile = tile;
                    bestFrame = frame;
                    bestDist = dist;
                }
            }
        }

        if (bestTile.isValid())
        {
#if DEBUG_SQUAD_TARGET
            CherryVis::log() << "MopUp cluster " << BWAPI::WalkPosition(cluster.center)
                             << ": moving to best tile @ " << BWAPI::WalkPosition(bestTile)
                             << "; frame seen: " << bestFrame;
#endif

            cluster.setActivity(UnitCluster::Activity::Moving);
            cluster.move(BWAPI::Position(bestTile) + BWAPI::Position(16, 16));
            return;
        }
    }

#if DEBUG_SQUAD_TARGET
    CherryVis::log() << "MopUp cluster " << BWAPI::WalkPosition(cluster.center) << ": Nothing to do!";
#endif
}
