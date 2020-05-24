#include "EarlyGameWorkerScout.h"

#include "Builder.h"
#include "Workers.h"
#include "Map.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Geo.h"
#include "Opponent.h"

#include <bwem.h>
#include <Strategist/Strategist.h>

#if INSTRUMENTATION_ENABLED
#define OUTPUT_SCOUTTILE_HEATMAP true
#endif

namespace
{
    const int goalWeight = 32;
    const double threatWeight = 128.0;

    const int mainPriority = 120;

    // Once we know the enemy main base, this method generates the map of prioritized tiles to scout
    // Top priority are mineral patches, geysers, and the area immediately around the resource depot
    // Next priority is the natural location
    // Lowest priority is all other tiles in the base area, unless the enemy is zerg (since they can only build on creep anyway)
    void generateTileScoutPriorities(const Base *base,
                                     std::map<int, std::set<BWAPI::TilePosition>> &scoutTiles,
                                     std::set<const BWEM::Area *> &scoutAreas)
    {
        // Determine the priorities to use
        // For Zerg we don't need to scout around the base (as they can only build on creep), but want to scout the natural more often
        int areaPriority = 480;
        int naturalPriority = 960;
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
        {
            areaPriority = 0;
            naturalPriority = 600;
        }

        auto tileValid = [](BWAPI::TilePosition tile, int neutralElevation)
        {
            if (!tile.isValid()) return false;
            if (!BWAPI::Broodwar->isBuildable(tile)) return false;
            if (BWAPI::Broodwar->getGroundHeight(tile) > neutralElevation) return false;

            return true;
        };

        int baseElevation = BWAPI::Broodwar->getGroundHeight(base->getTilePosition());

        // Start by assigning all tiles in the base area the lowest priority
        if (areaPriority)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    BWAPI::TilePosition here(x, y);
                    if (!tileValid(here, baseElevation)) continue;
                    if (BWEM::Map::Instance().GetArea(here) != base->getArea()) continue;

                    scoutTiles[areaPriority].insert(here);
                }
            }
        }

        // Now add tiles close to the natural
        auto natural = Map::getNaturalForStartLocation(base->getTilePosition());
        if (natural)
        {
            int naturalElevation = BWAPI::Broodwar->getGroundHeight(natural->getTilePosition());
            for (int x = -3; x < 7; x++)
            {
                for (int y = -3; y < 6; y++)
                {
                    auto here = natural->getTilePosition() + BWAPI::TilePosition(x, y);

                    if ((BWAPI::Position(here) + BWAPI::Position(16, 16)).getApproxDistance(natural->getPosition()) > 160) continue;

                    if (!tileValid(here, naturalElevation)) continue;

                    scoutTiles[naturalPriority].insert(here);
                }
            }
        }

        // Now add the tiles close to the depot, except for the mineral line
        for (int x = -6; x < 10; x++)
        {
            for (int y = -6; y < 9; y++)
            {
                auto here = base->getTilePosition() + BWAPI::TilePosition(x, y);

                if ((BWAPI::Position(here) + BWAPI::Position(16, 16)).getApproxDistance(base->getPosition()) > 250) continue;

                if (!tileValid(here, baseElevation)) continue;

                if (base->isInMineralLine(here)) continue;

                scoutTiles[mainPriority].insert(here);
            }
        }

        // Finally generate the areas covered by the scout
        scoutAreas.insert(base->getArea());
        if (natural)
        {
            auto path = PathFinding::GetChokePointPath(base->getPosition(), natural->getPosition());
            for (auto choke : path)
            {
                scoutAreas.insert(choke->GetAreas().first);
                scoutAreas.insert(choke->GetAreas().second);
            }
        }

#if OUTPUT_SCOUTTILE_HEATMAP
        std::vector<long> scoutTilesCvis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        int val = 10;
        for (auto it = scoutTiles.rbegin(); it != scoutTiles.rend(); it++)
        {
            for (auto &tile : it->second)
            {
                scoutTilesCvis[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] = val;
            }
            val += 10;
        }
        CherryVis::addHeatmap("WorkerScoutTiles", scoutTilesCvis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
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
        Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::ScoutingFailed);

        status.complete = true;
        return;
    }

    // Detect if the enemy is blocking our scout from getting into the target base
    if (isScoutBlocked())
    {
        // Assume the base is the enemy main if we haven't already inferred this
        Map::setEnemyStartingMain(targetBase);

        Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::ScoutingBlocked);

        status.complete = true;
        return;
    }

    // Wait until our target base is the enemy main
    if (targetBase != Map::getEnemyStartingMain()) return;

    // Determine the next tile we want to scout
    auto tile = getHighestPriorityScoutTile();
    if (!tile.isValid()) tile = BWAPI::TilePosition(targetBase->getPosition());

    // Determine when the scout has seen most of the highest-priority tiles
    if (Strategist::getWorkerScoutStatus() != Strategist::WorkerScoutStatus::EnemyBaseScouted && !scoutTiles.empty())
    {
        int seen = 0;
        for (auto &mainTile : scoutTiles.begin()->second)
        {
            if (Map::lastSeen(mainTile) > 0) seen++;
        }

        if ((double) seen / (double) scoutTiles.begin()->second.size() > 0.8)
        {
            Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::EnemyBaseScouted);
        }
    }

    // If we aren't already in a scout area, just move directly
    if (scoutAreas.find(BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(scout->lastPosition))) == scoutAreas.end())
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: out of scout areas, move directly to scout tile " << BWAPI::WalkPosition(tile);
#endif
        scout->moveTo(BWAPI::Position(tile) + BWAPI::Position(16, 16));
        return;
    }

    // Plot a path, avoiding threats and the enemy mineral line
    // Also reject tiles outside the scout areas to limit the search space
    auto grid = Players::grid(BWAPI::Broodwar->enemy());
    auto avoidThreatTiles = [&](BWAPI::TilePosition tile)
    {
        if (scoutAreas.find(BWEM::Map::Instance().GetNearestArea(tile)) == scoutAreas.end())
        {
            return false;
        }
        if (targetBase->isInMineralLine(tile)) return false;

        auto walk = BWAPI::WalkPosition(tile);
        for (int x = 0; x < 4; x++)
        {
            for (int y = 0; y < 4; y++)
            {
                if (grid.staticGroundThreat(walk + BWAPI::WalkPosition(x, y)) > 0) return false;
            }
        }
        return true;
    };
    auto closeEnoughToTarget = [&](BWAPI::TilePosition here)
    {
        return here.getApproxDistance(tile) < 6 && BWAPI::Broodwar->getGroundHeight(here) >= BWAPI::Broodwar->getGroundHeight(tile);
    };
    auto path = PathFinding::Search(scout->getTilePosition(), tile, avoidThreatTiles, closeEnoughToTarget);

    // Choose the appropriate target position
    BWAPI::Position targetPos;
    if (path.size() < 3)
    {
        targetPos = BWAPI::Position(tile) + BWAPI::Position(16, 16);

#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
    }
    else
    {
        targetPos = BWAPI::Position(path[2]) + BWAPI::Position(16, 16);

#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: target next path waypoint " << BWAPI::WalkPosition(targetPos);
#endif
    }

    // Compute threat avoidance boid
    int threatX = 0;
    int threatY = 0;
    for (auto &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (BWAPI::Broodwar->getFrameCount() - 120)) continue;
        if (!UnitUtil::CanAttackGround(unit->type)) continue;
        if (!unit->type.isBuilding() && unit->lastSeen < (BWAPI::Broodwar->getFrameCount() - 120)) continue;

        int detectionLimit = std::min(128, Players::weaponRange(unit->player, unit->type.groundWeapon()) + 64);
        int dist = scout->getDistance(unit);
        if (dist >= detectionLimit) continue;

        // Minimum force at detection limit, maximum force at detection limit - 64 (and closer)
        double distFactor = 1.0 - (double) std::max(0, dist - 64) / (double) (detectionLimit - 64);
        auto vector = BWAPI::Position(scout->lastPosition.x - unit->lastPosition.x, scout->lastPosition.y - unit->lastPosition.y);
        auto scaled = Geo::ScaleVector(vector, (int) (distFactor * threatWeight));

        threatX += scaled.x;
        threatY += scaled.y;
    }

    // If there are no threats, move directly
    if (threatX == 0 && threatY == 0)
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: No threats, moving directly to target " << BWAPI::WalkPosition(targetPos);
#endif

        scout->moveTo(targetPos, true);
        return;
    }

    // Compute goal boid
    int goalX;
    int goalY;
    {
        auto vector = BWAPI::Position(targetPos.x - scout->lastPosition.x, targetPos.y - scout->lastPosition.y);
        auto scaled = Geo::ScaleVector(vector, goalWeight);

        goalX = scaled.x;
        goalY = scaled.y;
    }

    // Put them all together and scale to two tiles
    BWAPI::Position totalVector;
    {
        auto vector = BWAPI::Position(goalX + threatX, goalY + threatY);
        totalVector = Geo::ScaleVector(vector, 64);
    }

    // Find a walkable position along the vector
    int dist = Geo::ApproximateDistance(0, totalVector.x, 0, totalVector.y) - 16;
    auto pos = scout->lastPosition + totalVector;
    while (dist > 0 && (!pos.isValid() || !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))))
    {
        pos = scout->lastPosition + Geo::ScaleVector(totalVector, dist);
        dist -= 16;
    }
    if (dist <= 0) pos = targetPos;

#if DEBUG_UNIT_ORDERS
    CherryVis::log(scout->id) << "Scouting boids towards " << BWAPI::WalkPosition(targetPos)
                              << ": goal=" << BWAPI::WalkPosition(scout->lastPosition + BWAPI::Position(goalX, goalY))
                              << "; threat=" << BWAPI::WalkPosition(scout->lastPosition + BWAPI::Position(threatX, threatY))
                              << "; total=" << BWAPI::WalkPosition(scout->lastPosition + totalVector)
                              << "; target=" << BWAPI::WalkPosition(pos);
#endif

    scout->moveTo(pos, true);
}

void EarlyGameWorkerScout::disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                                   const std::function<void(const MyUnit &)> &movableUnitCallback)
{
    if (scout && scout->exists())
    {
        Workers::releaseWorker(scout);
    }
}

bool EarlyGameWorkerScout::reserveScout()
{
    // Scout after the first gateway if playing a non-random opponent on a two-player map
    // In all other cases scout after the first pylon
    auto scoutAfterBuilding = BWAPI::UnitTypes::Protoss_Pylon;
    if (Map::getEnemyStartingMain() && !Opponent::isUnknownRace())
    {
        scoutAfterBuilding = BWAPI::UnitTypes::Protoss_Gateway;
    }

    for (auto &pendingBuilding : Builder::allPendingBuildings())
    {
        if (pendingBuilding->builder && pendingBuilding->type == scoutAfterBuilding)
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

    if (targetBase && targetBase == Map::getEnemyStartingMain())
    {
        Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::MovingToEnemyBase);
    }
    else if (targetBase)
    {
        Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::LookingForEnemyBase);
    }

    return true;
}

void EarlyGameWorkerScout::updateTargetBase()
{
    if (targetBase && targetBase == Map::getEnemyStartingMain()) return;
    if (targetBase && !targetBase->owner && targetBase->lastScouted == -1) return;

    targetBase = nullptr;
    closestDistanceToTargetBase = INT_MAX;
    lastForewardMotionFrame = BWAPI::Broodwar->getFrameCount();

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
            Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::MovingToEnemyBase);
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

bool EarlyGameWorkerScout::isScoutBlocked()
{
    // Not blocked if we got into the target base once
    // TODO: May want to detect blocking after we've been out to check the natural at some point, but for now we just handle initial blocking
    if (targetBase->lastScouted != -1) return false;

    // Get cost from here to the target base
    auto grid = PathFinding::getNavigationGrid(targetBase->getTilePosition());
    if (!grid) return false;
    auto node = (*grid)[scout->lastPosition];
    if (node.cost == USHRT_MAX) return false;

    // Decreasing distance is fine
    if (node.cost < closestDistanceToTargetBase)
    {
        closestDistanceToTargetBase = node.cost;
        lastForewardMotionFrame = BWAPI::Broodwar->getFrameCount();
        return false;
    }

    // Non-decreasing distance is fine if we are still far away from the enemy base
    if (node.cost > 1500) return false;

    // Consider us to be blocked if we haven't made forward progress in five seconds
    if ((BWAPI::Broodwar->getFrameCount() - lastForewardMotionFrame) > 120) return true;

    return false;

    /*

    Diabled as it doesn't really work - too many false positives and won't work if the enemy blocks such that we never see the unit.

    // Look for a stationary enemy unit nearby and in a narrow choke
    for (const auto &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (unit->getDistance(scout) > 100) continue;
        if (!Map::isInNarrowChoke(unit->getTilePosition())) continue;
        if (unit->isMoving) continue;

        Log::Get() << "Enemy scout blocker detected: " << *unit;
        return true;
    }
     */
}

BWAPI::TilePosition EarlyGameWorkerScout::getHighestPriorityScoutTile()
{
    if (scoutTiles.empty()) generateTileScoutPriorities(targetBase, scoutTiles, scoutAreas);

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
