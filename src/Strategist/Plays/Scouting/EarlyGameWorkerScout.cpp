#include "EarlyGameWorkerScout.h"

#include "Builder.h"
#include "Workers.h"
#include "Map.h"

#include <bwem.h>

#define SCOUTPRIORITY_BASEAREA 720 // Scout every 30 seconds
#define SCOUTPRIORITY_NATURAL 480 // Scout every 20 seconds
#define SCOUTPRIORITY_DEPOT 240 // Scout every 10 seconds

namespace
{
    // Once we know the enemy main base, this method generates the map of prioritized tiles to scout
    // Top priority are mineral patches, geysers, and the area immediately around the resource depot
    // Next priority is the natural location
    // Lowest priority is all other tiles in the base area
    void generateTileScoutPriorities(const Base *base, std::map<int, std::set<BWAPI::TilePosition>> &scoutTiles)
    {
        auto tileValid = [](BWAPI::TilePosition tile, int neutralElevation)
        {
            if (!tile.isValid()) return false;
            if (!BWAPI::Broodwar->isBuildable(tile)) return false;
            if (BWAPI::Broodwar->getGroundHeight(tile) > neutralElevation) return false;

            return true;
        };

        int baseElevation = BWAPI::Broodwar->getGroundHeight(base->getTilePosition());

        // Start by assigning all tiles in the base area the lowest priority
        for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
        {
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                BWAPI::TilePosition here(x, y);
                if (!tileValid(here, baseElevation)) continue;
                if (BWEM::Map::Instance().GetArea(here) == base->getArea())
                {
                    scoutTiles[SCOUTPRIORITY_BASEAREA].insert(here);
                }
            }
        }

        // Now add tiles close to the natural
        auto natural = Map::getNaturalForStartLocation(base->getTilePosition());
        if (natural)
        {
            int naturalElevation = BWAPI::Broodwar->getGroundHeight(natural->getTilePosition());
            for (int x = -6; x < 10; x++)
            {
                for (int y = -6; y < 9; y++)
                {
                    auto here = natural->getTilePosition() + BWAPI::TilePosition(x, y);
                    if (!tileValid(here, naturalElevation)) continue;

                    scoutTiles[SCOUTPRIORITY_NATURAL].insert(here);
                }
            }
        }

        // Now add the tiles close to the depot
        for (int x = -6; x < 10; x++)
        {
            for (int y = -6; y < 9; y++)
            {
                auto here = base->getTilePosition() + BWAPI::TilePosition(x, y);
                if (!tileValid(here, baseElevation)) continue;

                scoutTiles[SCOUTPRIORITY_DEPOT].insert(here);
            }
        }
    }
}

void EarlyGameWorkerScout::update()
{
    // Handle initial state where we haven't reserved the worker scout yet
    if (!scout && !reserveScout()) return;

    // Mark the play completed if the scout dies
    if (!scout->exists())
    {
        status.complete = true;
        return;
    }

    // Ensure the target base is assigned
    if (!targetBase)
    {
        if (!pickInitialTargetBase()) return;
    }
    else
    {
        updateTargetBase();
    }

    // If there is no target base here, our scout can't reach it, so this is probably an island map
    if (!targetBase)
    {
        status.complete = true;
        return;
    }

    // Wait until our target base is the enemy main
    if (targetBase != Map::getEnemyStartingMain()) return;

    auto tile = getHighestPriorityScoutTile();
    if (tile.isValid())
    {
#if DEBUG_UNIT_ORDERS
      CherryVis::log(scout->id) << "Scout: scout tile " << (BWAPI::WalkPosition(tile) + BWAPI::WalkPosition(2, 2));
#endif
        scout->moveTo(BWAPI::Position(tile) + BWAPI::Position(16, 16));
    }
}

bool EarlyGameWorkerScout::reserveScout()
{
    // We take the worker that builds the first pylon
    for (auto &pendingBuilding : Builder::allPendingBuildings())
    {
        if (pendingBuilding->builder && pendingBuilding->type == BWAPI::UnitTypes::Protoss_Pylon)
        {
            scout = pendingBuilding->builder;
            return true;
        }
    }

    return false;
}

bool EarlyGameWorkerScout::pickInitialTargetBase()
{
    // Wait for the worker to be available for reassignment
    if (!Workers::isAvailableForReassignment(scout, false)) return false;

    // Reserve the worker
    Workers::reserveWorker(scout);

    updateTargetBase();
    return true;
}

void EarlyGameWorkerScout::updateTargetBase()
{
    if (targetBase && targetBase == Map::getEnemyStartingMain()) return;
    if (targetBase && !targetBase->owner && targetBase->lastScouted == -1) return;

    targetBase = nullptr;

    // Assign the enemy starting main if we know it
    if (Map::getEnemyStartingMain())
    {
        // Verify that it is actually reachable, this might be an island map
        if (PathFinding::GetGroundDistance(scout->lastPosition,
                                           Map::getEnemyStartingMain()->getPosition(),
                                           scout->type,
                                           PathFinding::PathFindingOptions::UseNearestBWEMArea) >= 0)
        {
            targetBase = Map::getEnemyStartingMain();
        }
    }
    else
    {
        // Pick the closest remaining base to the worker
        // TODO: On four player maps with buildable centers, it may be a good idea to cross the center to uncover proxies
        int bestTravelTime = INT_MAX;
        for (auto base : Map::unscoutedStartingLocations())
        {
            int travelTime =
                    PathFinding::ExpectedTravelTime(scout->lastPosition,
                                                    base->getPosition(),
                                                    scout->type,
                                                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (travelTime != -1 && travelTime < bestTravelTime)
            {
                bestTravelTime = travelTime;
                targetBase = base;
            }
        }
    }

    // Move towards the base
    if (targetBase)
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "moveTo: Scout base @ " << BWAPI::WalkPosition(targetBase->getPosition());
#endif
        scout->moveTo(targetBase->getPosition());
    }
}

BWAPI::TilePosition EarlyGameWorkerScout::getHighestPriorityScoutTile()
{
    if (scoutTiles.empty()) generateTileScoutPriorities(targetBase, scoutTiles);

    int highestPriorityFrame = INT_MAX;
    int highestPriorityDist = INT_MAX;
    BWAPI::TilePosition highestPriority = BWAPI::TilePositions::Invalid;

    for (auto &priorityAndTiles : scoutTiles)
    {
        for (auto &tile : priorityAndTiles.second)
        {
            int desiredFrame = Map::lastSeen(tile) + priorityAndTiles.first;
            if (desiredFrame > highestPriorityFrame) continue;

            int dist = scout->getDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16));

            if (desiredFrame < highestPriorityFrame || dist < highestPriorityDist)
            {
                highestPriorityFrame = desiredFrame;
                highestPriorityDist = dist;
                highestPriority = tile;
            }
        }
    }

    return highestPriority;
}