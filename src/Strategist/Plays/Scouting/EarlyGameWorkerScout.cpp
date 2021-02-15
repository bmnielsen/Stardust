#include "EarlyGameWorkerScout.h"

#include "Builder.h"
#include "Workers.h"
#include "Map.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Geo.h"
#include "Boids.h"
#include "Opponent.h"
#include "Strategist.h"

#include "DebugFlag_UnitOrders.h"

#include <bwem.h>

#if INSTRUMENTATION_ENABLED
#define OUTPUT_SCOUTTILE_HEATMAP false
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
    void generateTileScoutPriorities(std::map<int, std::set<BWAPI::TilePosition>> &scoutTiles,
                                     std::set<const BWEM::Area *> &scoutAreas)
    {
        auto base = Map::getEnemyStartingMain();
        if (!base) return;

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
        auto natural = Map::getEnemyStartingNatural();
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

    BWAPI::TilePosition getTileToHideOn()
    {
        auto enemyMain = Map::getEnemyStartingMain();
        if (!enemyMain) return BWAPI::TilePositions::Invalid;

        // Collect all of the tiles the enemy has vision on
        std::set<BWAPI::TilePosition> visionTiles;
        for (const auto &unit : Units::allEnemy())
        {
            if (!unit->type.isBuilding()) continue;

            auto topLeft = unit->getTilePosition();

            int tileSightRadius = (unit->type.sightRange() + 31) / 32;
            for (int x = -tileSightRadius; x < unit->type.tileWidth() + tileSightRadius; x++)
            {
                for (int y = -tileSightRadius; y < unit->type.tileHeight() + tileSightRadius; y++)
                {
                    auto here = topLeft + BWAPI::TilePosition(x, y);
                    if (!here.isValid()) continue;

                    if (Geo::EdgeToTileDistance(unit->type, topLeft, here) <= tileSightRadius)
                    {
                        visionTiles.insert(here);
                    }
                }
            }
        }

        // Now get the best tile in the main area
        auto enemyChoke = Map::getEnemyMainChoke();
        int baseElevation = BWAPI::Broodwar->getGroundHeight(enemyMain->getTilePosition());
        int bestDist = 0;
        BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                BWAPI::TilePosition here(x, y);
                if (!here.isValid()) continue;
                if (!Map::isWalkable(here)) continue;
                if (BWAPI::Broodwar->getGroundHeight(here) > baseElevation) continue;
                if (BWEM::Map::Instance().GetArea(here) != enemyMain->getArea()) continue;

                int dist = enemyMain->getPosition().getApproxDistance(BWAPI::Position(here) + BWAPI::Position(16, 16));
                if (enemyChoke)
                {
                    dist += enemyChoke->center.getApproxDistance(BWAPI::Position(here) + BWAPI::Position(16, 16));
                }

                if (dist > bestDist)
                {
                    bestDist = dist;
                    bestTile = here;
                }
            }
        }

        return bestTile;
    }

    BWAPI::TilePosition getTileToMonitorChokeFrom()
    {
        auto enemyMainChoke = Map::getEnemyMainChoke();
        if (!enemyMainChoke) return BWAPI::TilePositions::Invalid;

        // We measure potential tiles against the natural choke by default, or our main if a natural choke doesn't exist
        auto referencePosition = Map::getMyMain()->getPosition();
        auto chokePath = PathFinding::GetChokePointPath(referencePosition,
                                                        enemyMainChoke->center,
                                                        BWAPI::UnitTypes::Protoss_Dragoon,
                                                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
        if (!chokePath.empty())
        {
            auto it = chokePath.rbegin();
            auto naturalChoke = Map::choke(*it);

            // Depending on the choke geography we might get the choke itself as the last part of the path
            // If so, advance to the next one
            if (naturalChoke == enemyMainChoke)
            {
                it++;
                if (it != chokePath.rend()) naturalChoke = Map::choke(*it);
            }

            if (naturalChoke)
            {
                referencePosition = naturalChoke->center;
            }
        }

        // Use our natural's height as the reference height
        auto referenceHeight = -1;
        if (Map::getEnemyStartingNatural())
        {
            referenceHeight = BWAPI::Broodwar->getGroundHeight(Map::getEnemyStartingNatural()->getTilePosition());
        }

        auto tileSightRange = BWAPI::UnitTypes::Protoss_Probe.sightRange() / 32;
        int bestDist = INT_MAX;
        BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;
        for (int y = -tileSightRange + 1; y < tileSightRange; y++)
        {
            for (int x = -tileSightRange + 1; x < tileSightRange; x++)
            {
                auto here = BWAPI::TilePosition(enemyMainChoke->center) + BWAPI::TilePosition(x, y);
                if (!here.isValid()) continue;
                if (!Map::isWalkable(here)) continue;

                if (referenceHeight != -1 && BWAPI::Broodwar->getGroundHeight(here) != referenceHeight) continue;

                int chokeDist = enemyMainChoke->center.getApproxDistance(BWAPI::Position(here) + BWAPI::Position(16, 16));
                if (chokeDist > BWAPI::UnitTypes::Protoss_Probe.sightRange()) continue;

                int dist = referencePosition.getApproxDistance(BWAPI::Position(here) + BWAPI::Position(16, 16));
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestTile = here;
                }
            }
        }

        return bestTile;
    }
}

void EarlyGameWorkerScout::update()
{
    // Handle initial state where we haven't reserved the worker scout yet
    if (!scout && !reserveScout()) return;

    // Mark the play completed if the scout dies
    if (!scout->exists())
    {
        if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted ||
            Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::MonitoringEnemyChoke)
        {
            Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::ScoutingCompleted);
        }
        else
        {
            Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::ScoutingFailed);
        }

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

    // Wait until our target base is the enemy main and we have scouted it once
    if (targetBase != Map::getEnemyStartingMain() || targetBase->lastScouted == -1) return;

    // Determine when the scout has seen most of the highest-priority tiles
    if (Strategist::getWorkerScoutStatus() != Strategist::WorkerScoutStatus::EnemyBaseScouted &&
        Strategist::getWorkerScoutStatus() != Strategist::WorkerScoutStatus::MonitoringEnemyChoke &&
        !scoutTiles.empty())
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

    // Determine the next tile we want to scout or move to
    BWAPI::TilePosition tile;
    if (hidingUntil > BWAPI::Broodwar->getFrameCount())
    {
        tile = getTileToHideOn();
    }
    else if (fixedPosition.isValid())
    {
        tile = fixedPosition;
        if (scout->getDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16)) < 64)
        {
            Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::MonitoringEnemyChoke);
        }
        else
        {
            Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::EnemyBaseScouted);
        }
    }
    else
    {
        tile = getHighestPriorityScoutTile();
    }
    if (!tile.isValid()) tile = BWAPI::TilePosition(targetBase->getPosition());

    // Compute threat avoidance boid
    int threatX = 0;
    int threatY = 0;
    bool hasThreat = false;
    bool hasRangedThreat = false;
    for (auto &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (BWAPI::Broodwar->getFrameCount() - 120)) continue;
        if (!UnitUtil::CanAttackGround(unit->type)) continue;
        if (!unit->type.isBuilding() && unit->lastSeen < (BWAPI::Broodwar->getFrameCount() - 120)) continue;

        int detectionLimit = std::max(128, unit->groundRange() + 64);
        int dist = scout->getDistance(unit);
        if (dist >= detectionLimit) continue;

        hasThreat = true;

        // If the enemy has a ranged unit inside our detection limit, skip threat avaoidance entirely
        // Rationale is that we can't get away anyway, so let's just get the scouting done that we can before dying
        if (UnitUtil::IsRangedUnit(unit->type))
        {
            hasRangedThreat = true;
            break;
        }

        // Minimum force at detection limit, maximum force at detection limit - 64 (and closer)
        double distFactor = 1.0 - (double) std::max(0, dist - 64) / (double) (detectionLimit - 64);
        auto vector = BWAPI::Position(scout->lastPosition.x - unit->lastPosition.x, scout->lastPosition.y - unit->lastPosition.y);
        auto scaled = Geo::ScaleVector(vector, (int) (distFactor * threatWeight));

        threatX += scaled.x;
        threatY += scaled.y;
    }

    // Get the next waypoint
    BWAPI::Position targetPos;

    // If we are outside the scout areas, use the navigation grid
    if (scoutAreas.find(BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(scout->lastPosition))) == scoutAreas.end())
    {
        // Move directly if there is no enemy threat or a ranged threat
        if (!hasThreat || hasRangedThreat)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(scout->id) << "Scout: out of scout areas, move directly to scout tile " << BWAPI::WalkPosition(tile);
#endif
            scout->moveTo(BWAPI::Position(tile) + BWAPI::Position(16, 16));
            return;
        }

        auto navigationGrid = PathFinding::getNavigationGrid(targetBase->getTilePosition());
        auto node = navigationGrid ? &(*navigationGrid)[scout->getTilePosition()] : nullptr;
        node = node ? node->nextNode : nullptr;
        node = node ? node->nextNode : nullptr;
        if (!node)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(scout->id) << "Scout: out of scout areas and no valid navigation grid node, move directly to scout tile "
                                      << BWAPI::WalkPosition(tile);
#endif
            scout->moveTo(BWAPI::Position(tile) + BWAPI::Position(16, 16));
            return;
        }

        targetPos = node->center();
    }
    else
    {
        // Plot a path, avoiding static defenses and the enemy mineral line
        // Also reject tiles outside the scout areas to limit the search space
        auto grid = Players::grid(BWAPI::Broodwar->enemy());
        auto avoidThreatTiles = [&](BWAPI::TilePosition tile)
        {
            if (!Map::isWalkable(tile)) return false;
            if (scoutAreas.find(BWEM::Map::Instance().GetNearestArea(tile)) == scoutAreas.end())
            {
                return false;
            }
            if (targetBase->isInMineralLine(tile)) return false;

            auto walk = BWAPI::WalkPosition(tile);
            for (int y = 0; y < 4; y++)
            {
                for (int x = 0; x < 4; x++)
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
    }

    // If there is a ranged threat or no threats, move directly
    if (!hasThreat || hasRangedThreat)
    {
#if DEBUG_UNIT_ORDERS
        if (hasRangedThreat)
        {
            CherryVis::log(scout->id) << "Scout: Ranged threat, moving directly to target " << BWAPI::WalkPosition(targetPos);
        }
        else
        {
            CherryVis::log(scout->id) << "Scout: No threats, moving directly to target " << BWAPI::WalkPosition(targetPos);
        }
#endif

        scout->moveTo(targetPos, true);
        return;
    }

    // Compute goal boid
    int goalX = 0;
    int goalY = 0;
    {
        auto vector = BWAPI::Position(targetPos.x - scout->lastPosition.x, targetPos.y - scout->lastPosition.y);
        auto scaled = Geo::ScaleVector(vector, goalWeight);
        if (scaled != BWAPI::Positions::Invalid)
        {
            goalX = scaled.x;
            goalY = scaled.y;
        }
    }

    auto pos = Boids::ComputePosition(scout.get(), {goalX, threatX}, {goalY, threatY}, 64, 16, 3);

#if DEBUG_UNIT_BOIDS
    CherryVis::log(scout->id) << "Scouting boids towards " << BWAPI::WalkPosition(targetPos)
                              << ": goal=" << BWAPI::WalkPosition(scout->lastPosition + BWAPI::Position(goalX, goalY))
                              << "; threat=" << BWAPI::WalkPosition(scout->lastPosition + BWAPI::Position(threatX, threatY))
                              << "; total=" << BWAPI::WalkPosition(scout->lastPosition + BWAPI::Position(goalX + threatX, goalY + threatY))
                              << "; target=" << BWAPI::WalkPosition(pos);
#endif

    // Default to target pos if unit can't move in the desired direction
    if (pos == BWAPI::Positions::Invalid) pos = targetPos;

    scout->moveTo(pos, true);
}

void EarlyGameWorkerScout::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                                   const std::function<void(const MyUnit)> &movableUnitCallback)
{
    if (scout && scout->exists())
    {
        CherryVis::log(scout->id) << "Releasing from non-mining duties (scout disband)";
        Workers::releaseWorker(scout);
    }
}

void EarlyGameWorkerScout::hideUntil(int frame)
{
#if INSTRUMENTATION_ENABLED
    if (frame != hidingUntil)
    {
        CherryVis::log() << "Hiding worker scout until frame " << frame;
    }
#endif

    hidingUntil = frame;
}

void EarlyGameWorkerScout::monitorEnemyChoke()
{
    fixedPosition = getTileToMonitorChokeFrom();
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

    // If a building of the required type already exists, this play is being added late
    // This happens vs. random when the play is re-initialized once the enemy race is known
    // In this case grab our furthest worker from the main base, as it is likely to have already been our scout
    if (Units::countAll(scoutAfterBuilding) > 0)
    {
        int bestDist = 0;
        for (const auto &worker : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
        {
            int dist = PathFinding::GetGroundDistance(worker->lastPosition,
                                                      Map::getMyMain()->getPosition(),
                                                      BWAPI::UnitTypes::Protoss_Probe,
                                                      PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (dist > bestDist)
            {
                bestDist = dist;
                scout = worker;
            }
        }

        return scout != nullptr;
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
    lastDistanceToTargetBase = INT_MAX;
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
                                                    PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                    -1);
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
    if (node.cost == USHRT_MAX)
    {
        // This means either that our scout is in an unexpected position or the enemy has done a wall-in
        // Check for the latter by checking if there is a path from the target base to our main
        auto mainGrid = PathFinding::getNavigationGrid(Map::getMyMain()->getTilePosition());
        if (!mainGrid) return false;

        auto targetNode = (*mainGrid)[targetBase->getTilePosition()];
        if (targetNode.cost == USHRT_MAX) return true;

        return false;
    }

    // Decreasing distance is fine
    // Increasing distance is fine if it jumps quite a bit - this generally means we've scouted a building that changes the path
    if (node.cost<closestDistanceToTargetBase ||
                  node.cost>(lastDistanceToTargetBase + 100))
    {
        closestDistanceToTargetBase = node.cost;
        lastDistanceToTargetBase = node.cost;
        lastForewardMotionFrame = BWAPI::Broodwar->getFrameCount();
        return false;
    }

    lastDistanceToTargetBase = node.cost;

    // Non-decreasing distance is fine if we are still far away from the enemy base
    if (node.cost > 3000) return false;

    // Consider us to be blocked if we haven't made forward progress in five seconds
    if ((BWAPI::Broodwar->getFrameCount() - lastForewardMotionFrame) > 120) return true;

    return false;
}

BWAPI::TilePosition EarlyGameWorkerScout::getHighestPriorityScoutTile()
{
    if (scoutTiles.empty()) generateTileScoutPriorities(scoutTiles, scoutAreas);

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
