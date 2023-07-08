#include "EarlyGameWorkerScout.h"

#include "Builder.h"
#include "Workers.h"
#include "Map.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Geo.h"
#include "Boids.h"
#include "Strategist.h"
#include "Strategies.h"
#include "OpponentEconomicModel.h"

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

#if OUTPUT_SCOUTTILE_HEATMAP
    std::vector<long> lastScoutTilesCvis;
#endif

    MyUnit getScoutAfterBuilding(BWAPI::UnitType buildingType, int buildingCount, const MyUnit &existingScout = nullptr)
    {
        // If a building of the required type already exists, this play is being added late
        // This happens vs. random when the play is re-initialized once the enemy race is known
        // In this case grab our furthest worker from the main base, as it is likely to have already been our scout
        if (Units::countAll(buildingType) >= buildingCount)
        {
            MyUnit bestWorker = nullptr;
            int bestDist = 0;
            for (const auto &worker : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (existingScout && existingScout->id == worker->id) continue;

                int dist = PathFinding::GetGroundDistance(worker->lastPosition,
                                                          Map::getMyMain()->getPosition(),
                                                          BWAPI::UnitTypes::Protoss_Probe,
                                                          PathFinding::PathFindingOptions::UseNearestBWEMArea);
                if (dist > bestDist)
                {
                    bestDist = dist;
                    bestWorker = worker;
                }
            }

            return bestWorker;
        }

        for (auto &pendingBuilding : Builder::allPendingBuildings())
        {
            if (pendingBuilding->builder && pendingBuilding->type == buildingType)
            {
                return pendingBuilding->builder;
            }
        }

        return nullptr;
    }

    void updateTargetBase(EarlyGameWorkerScout::ScoutLookingForEnemyBase &scout, EarlyGameWorkerScout::ScoutLookingForEnemyBase &otherScout)
    {
        // Reserve the scout if it hasn't already been done
        if (!scout.reserved)
        {
            // Wait for the worker to be available for reassignment
            if (!Workers::isAvailableForReassignment(scout.unit, false)) return;

            // Reserve the worker
            Workers::reserveWorker(scout.unit);
            scout.reserved = true;
        }

        // Jump out if this scout is already assigned to a valid base
        if (scout.targetBase && scout.targetBase == Map::getEnemyStartingMain()) return;
        if (scout.targetBase && !scout.targetBase->owner && scout.targetBase->lastScouted == -1) return;

        // Reset before picking a new base
        scout.targetBase = nullptr;
        scout.closestDistanceToTargetBase = INT_MAX;
        scout.lastDistanceToTargetBase = INT_MAX;
        scout.lastForwardMotionFrame = currentFrame;

        // Pick the closest remaining base to the worker that isn't assigned to another scout
        // TODO: On four player maps with buildable centers, it may be a good idea to cross the center to uncover proxies
        Base *bestBase = nullptr;
        int bestTravelTime = INT_MAX;
        for (auto base : Map::unscoutedStartingLocations())
        {
            if (otherScout.targetBase == base) continue;

            int travelTime =
                    PathFinding::ExpectedTravelTime(scout.unit->lastPosition,
                                                    base->getPosition(),
                                                    scout.unit->type,
                                                    PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                    1.1,
                                                    -1);
            if (travelTime != -1 && travelTime < bestTravelTime)
            {
                bestTravelTime = travelTime;
                bestBase = base;
            }
        }

        // Move towards the base
        if (bestBase)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(scout.unit->id) << "moveTo: Scout base @ " << BWAPI::WalkPosition(bestBase->getPosition());
#endif
            scout.unit->moveTo(bestBase->getPosition());
            scout.targetBase = bestBase;
            if (bestBase == Map::getEnemyStartingMain())
            {
                Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::MovingToEnemyBase);
            }
            else
            {
                Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::LookingForEnemyBase);
            }
        }
    }

    bool isScoutBlocked(EarlyGameWorkerScout::ScoutLookingForEnemyBase &scout)
    {
        if (!scout.unit || !scout.reserved || !scout.targetBase) return false;

        // Not blocked if we got into the target base once
        // TODO: May want to detect blocking after we've been out to check the natural at some point, but for now we just handle initial blocking
        if (scout.targetBase->lastScouted != -1) return false;

        // Get cost from here to the target base
        auto grid = PathFinding::getNavigationGrid(scout.targetBase->getPosition());
        if (!grid) return false;
        auto node = (*grid)[scout.unit->lastPosition];
        if (node.cost == USHRT_MAX)
        {
            // This means either that our scout is in an unexpected position or the enemy has done a wall-in
            // Check for the latter by checking if there is a path from the target base to our main
            auto mainGrid = PathFinding::getNavigationGrid(Map::getMyMain()->getPosition());
            if (!mainGrid) return false;

            auto targetNode = (*mainGrid)[scout.targetBase->getTilePosition()];
            if (targetNode.cost == USHRT_MAX)
            {
                Map::setEnemyStartingMain(scout.targetBase);
                return true;
            }

            return false;
        }

        // Decreasing distance is fine
        // Increasing distance is fine if it jumps quite a bit - this generally means we've scouted a building that changes the path
        if (node.cost<scout.closestDistanceToTargetBase || node.cost>(scout.lastDistanceToTargetBase + 100))
        {
            scout.closestDistanceToTargetBase = node.cost;
            scout.lastDistanceToTargetBase = node.cost;
            scout.lastForwardMotionFrame = currentFrame;
            return false;
        }

        scout.lastDistanceToTargetBase = node.cost;

        // Non-decreasing distance is fine if we are still far away from the enemy base
        if (node.cost > 3000) return false;

        // Consider us to be blocked if we haven't made forward progress in five seconds
        if ((currentFrame - scout.lastForwardMotionFrame) > 120)
        {
            Map::setEnemyStartingMain(scout.targetBase);
            return true;
        }

        return false;
    }

    // Once we know the enemy main base, this method generates the map of prioritized tiles to scout
    // Top priority are mineral patches, geysers, and the area immediately around the resource depot
    // Next priority is the natural location
    // Lowest priority is all other tiles in the base area, unless the enemy is zerg (since they can only build on creep anyway)
    void generateTileScoutPriorities(std::map<int, std::set<BWAPI::TilePosition>> &scoutTiles,
                                     std::set<const BWEM::Area *> &scoutAreas)
    {
        auto base = Map::getEnemyStartingMain();
        if (!base) return;

        scoutTiles.clear();

        // Determine the priorities to use
        int areaPriority = 800;
        int naturalPriority = 960;

        // For Zerg we don't need to scout around the base (as they can only build on creep), but want to scout the natural more often
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
        {
            areaPriority = 0;
            naturalPriority = 600;
        }

        // If we suspect a proxy, scout the natural and outskirts more aggressively early on in case we missed something
        if (Strategist::getStrategyEngine()->isEnemyProxy() && currentFrame < 4000)
        {
            naturalPriority = 480;
            areaPriority = 600;
        }

        // If the enemy is rushing, give the main area and natural much lower priority
        // We mainly want to keep an eye on when they start transitioning out of the rush
        else if (Strategist::getStrategyEngine()->isEnemyRushing() && BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg)
        {
            areaPriority = 1200;
            naturalPriority = 1200;
        }

        // If the enemy is protoss, don't scout the natural if our economic model tells us they can't have taken an extra nexus yet
        else if (OpponentEconomicModel::enabled() &&
            OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Nexus) > currentFrame)
        {
            naturalPriority = 0;
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
        if (natural && naturalPriority)
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
        for (auto it = scoutTiles.rbegin(); it != scoutTiles.rend(); it++)
        {
            for (auto &tile : it->second)
            {
                scoutTilesCvis[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] = it->first;
            }
        }
        if (lastScoutTilesCvis != scoutTilesCvis)
        {
            CherryVis::addHeatmap("WorkerScoutTiles", scoutTilesCvis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
        }
        lastScoutTilesCvis = scoutTilesCvis;
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
    if (Map::getEnemyStartingMain() && Map::getEnemyStartingMain()->lastScouted != -1)
    {
        scoutEnemyBase();
    }
    else
    {
        findEnemyBase();
    }
}


void EarlyGameWorkerScout::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                                   const std::function<void(const MyUnit)> &movableUnitCallback)
{
    auto releaseScout = [](const EarlyGameWorkerScout::ScoutLookingForEnemyBase &s)
    {
        if (s.unit && s.unit->exists())
        {
            CherryVis::log(s.unit->id) << "Releasing from non-mining duties (scout disband)";
            Workers::releaseWorker(s.unit);
        }
    };
    releaseScout(scout);
    releaseScout(secondScout);

    if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted ||
        Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::MonitoringEnemyChoke)
    {
        Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::ScoutingCompleted);
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

void EarlyGameWorkerScout::findEnemyBase()
{
    // Handle initial state where we haven't reserved the worker scout yet
    if (!scout.unit && !selectScout()) return;

    if (!secondScout.unit) selectSecondScout();

    // Mark the play completed if one of the scouts die
    if (!scout.unit->exists() || (secondScout.unit && !secondScout.unit->exists()))
    {
        Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::ScoutingFailed);
        status.complete = true;
        return;
    }

    // Update the target base
    updateTargetBase(scout, secondScout);
    updateTargetBase(secondScout, scout);

    // Handle case where no base can be scouted
    // This indicates an island map
    if (scout.reserved && !scout.targetBase && (!secondScout.reserved || !secondScout.targetBase))
    {
        Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::ScoutingFailed);

        status.complete = true;
        return;
    }

    // Handle case where one scout has nothing left to do
    if (scout.reserved && !scout.targetBase)
    {
        // Second scout takes over for the first
        CherryVis::log(scout.unit->id) << "Releasing from non-mining duties (other scout takes over)";
        Workers::releaseWorker(scout.unit);

        scout.unit = secondScout.unit;
        scout.targetBase = secondScout.targetBase;
        scout.closestDistanceToTargetBase = secondScout.closestDistanceToTargetBase;
        scout.lastDistanceToTargetBase = secondScout.lastDistanceToTargetBase;
        scout.lastForwardMotionFrame = secondScout.lastForwardMotionFrame;

        secondScout.unit = nullptr;
        secondScout.reserved = false;
        secondScout.targetBase = nullptr;
    }
    if (secondScout.reserved && !secondScout.targetBase)
    {
        // Second scout is no longer needed
        CherryVis::log(secondScout.unit->id) << "Releasing from non-mining duties (other scout takes over)";
        Workers::releaseWorker(secondScout.unit);

        secondScout.unit = nullptr;
        secondScout.reserved = false;
        secondScout.targetBase = nullptr;
    }

    // Detect if the enemy is blocking our scout from getting into the target base
    if (isScoutBlocked(scout) || isScoutBlocked(secondScout))
    {
        Strategist::setWorkerScoutStatus(Strategist::WorkerScoutStatus::ScoutingBlocked);

        status.complete = true;
        return;
    }
}

void EarlyGameWorkerScout::scoutEnemyBase()
{
    // If we still have two scouts, release one
    if (secondScout.unit)
    {
        if (!secondScout.reserved)
        {
            secondScout.unit = nullptr;
        }
        else
        {
            // Swap if the second scout found the base
            if (secondScout.targetBase == Map::getEnemyStartingMain())
            {
                CherryVis::log(scout.unit->id) << "Releasing from non-mining duties (other scout takes over)";
                Workers::releaseWorker(scout.unit);

                scout.unit = secondScout.unit;
                scout.targetBase = secondScout.targetBase;
                scout.closestDistanceToTargetBase = secondScout.closestDistanceToTargetBase;
                scout.lastDistanceToTargetBase = secondScout.lastDistanceToTargetBase;
                scout.lastForwardMotionFrame = secondScout.lastForwardMotionFrame;

                secondScout.unit = nullptr;
                secondScout.reserved = false;
                secondScout.targetBase = nullptr;
            }
            else
            {
                // Second scout is no longer needed
                CherryVis::log(secondScout.unit->id) << "Releasing from non-mining duties (other scout takes over)";
                Workers::releaseWorker(secondScout.unit);

                secondScout.unit = nullptr;
                secondScout.reserved = false;
                secondScout.targetBase = nullptr;
            }
        }
    }

    // Mark the play completed if the scout dies
    if (!scout.unit->exists())
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
    if (hidingUntil > currentFrame)
    {
        tile = getTileToHideOn();
    }
    else if (fixedPosition.isValid())
    {
        tile = fixedPosition;
        if (scout.unit->getDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16)) < 64)
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
    if (!tile.isValid()) tile = BWAPI::TilePosition(scout.targetBase->getPosition());

    // Compute threat avoidance boid
    int threatX = 0;
    int threatY = 0;
    bool hasThreat = false;
    bool hasRangedThreat = false;
    for (auto &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (currentFrame - 120)) continue;
        if (!UnitUtil::CanAttackGround(unit->type)) continue;
        if (!unit->type.isBuilding() && unit->lastSeen < (currentFrame - 120)) continue;
        if (!unit->completed) continue;

        int detectionLimit = std::max(128, unit->groundRange() + 32);
        int dist = scout.unit->getDistance(unit);
        if (dist >= detectionLimit) continue;

        hasThreat = true;

        // If the enemy has a ranged unit inside our detection limit, skip threat avaoidance entirely
        // Rationale is that we can't get away anyway, so let's just get the scouting done that we can before dying
        if (UnitUtil::IsRangedUnit(unit->type))
        {
            hasRangedThreat = true;
            break;
        }

        // Minimum force at detection limit, maximum force at detection limit - 32 (and closer)
        double distFactor = 1.0 - (double) std::max(0, dist - 32) / (double) (detectionLimit - 32);
        auto vector = BWAPI::Position(scout.unit->lastPosition.x - unit->lastPosition.x, scout.unit->lastPosition.y - unit->lastPosition.y);
        auto scaled = Geo::ScaleVector(vector, (int) (distFactor * threatWeight));

        threatX += scaled.x;
        threatY += scaled.y;
    }

    // If we have scouted the base at least once and the enemy has a ranged unit, complete the play
    if (hasRangedThreat && Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted)
    {
        status.complete = true;
        return;
    }

    // Get the next waypoint
    BWAPI::Position targetPos;

    // If we are outside the scout areas, use the navigation grid
    if (scoutAreas.find(BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(scout.unit->lastPosition))) == scoutAreas.end())
    {
        // Move directly if there is no enemy threat or a ranged threat
        if (!hasThreat || hasRangedThreat)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(scout.unit->id) << "Scout: out of scout areas, move directly to scout tile " << BWAPI::WalkPosition(tile);
#endif
            scout.unit->moveTo(BWAPI::Position(tile) + BWAPI::Position(16, 16));
            return;
        }

        auto navigationGrid = PathFinding::getNavigationGrid(scout.targetBase->getPosition());
        auto node = navigationGrid ? &(*navigationGrid)[scout.unit->getTilePosition()] : nullptr;
        node = node ? node->nextNode : nullptr;
        node = node ? node->nextNode : nullptr;
        if (!node)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(scout.unit->id) << "Scout: out of scout areas and no valid navigation grid node, move directly to scout tile "
                                      << BWAPI::WalkPosition(tile);
#endif
            scout.unit->moveTo(BWAPI::Position(tile) + BWAPI::Position(16, 16));
            return;
        }

        targetPos = node->center();
    }
    else
    {
        // Plot a path, avoiding static defenses and the enemy mineral line
        // Also reject tiles outside the scout areas to limit the search space
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        auto avoidThreatTiles = [&](BWAPI::TilePosition tile)
        {
            if (!Map::isWalkable(tile)) return false;
            if (scoutAreas.find(BWEM::Map::Instance().GetNearestArea(tile)) == scoutAreas.end())
            {
                return false;
            }
            if (scout.targetBase->isInMineralLine(tile)) return false;

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
        auto path = PathFinding::Search(scout.unit->getTilePosition(), tile, avoidThreatTiles, closeEnoughToTarget);

        // Choose the appropriate target position
        if (path.size() < 3)
        {
            targetPos = BWAPI::Position(tile) + BWAPI::Position(16, 16);

#if DEBUG_UNIT_ORDERS
            CherryVis::log(scout.unit->id) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
        }
        else
        {
            targetPos = BWAPI::Position(path[2]) + BWAPI::Position(16, 16);

#if DEBUG_UNIT_ORDERS
            CherryVis::log(scout.unit->id) << "Scout: target next path waypoint " << BWAPI::WalkPosition(targetPos);
#endif
        }
    }

    // If there is a ranged threat or no threats, move directly
    if (!hasThreat || hasRangedThreat)
    {
#if DEBUG_UNIT_ORDERS
        if (hasRangedThreat)
        {
            CherryVis::log(scout.unit->id) << "Scout: Ranged threat, moving directly to target " << BWAPI::WalkPosition(targetPos);
        }
        else
        {
            CherryVis::log(scout.unit->id) << "Scout: No threats, moving directly to target " << BWAPI::WalkPosition(targetPos);
        }
#endif

        scout.unit->moveTo(targetPos, true);
        return;
    }

    // Compute goal boid
    int goalX = 0;
    int goalY = 0;
    {
        auto vector = BWAPI::Position(targetPos.x - scout.unit->lastPosition.x, targetPos.y - scout.unit->lastPosition.y);
        auto scaled = Geo::ScaleVector(vector, goalWeight);
        if (scaled != BWAPI::Positions::Invalid)
        {
            goalX = scaled.x;
            goalY = scaled.y;
        }
    }

    auto pos = Boids::ComputePosition(scout.unit.get(), {goalX, threatX}, {goalY, threatY}, 96, UnitUtil::HaltDistance(scout.unit->type) + 16, true);

#if DEBUG_UNIT_BOIDS
    CherryVis::log(scout.unit->id) << "Scouting boids towards " << BWAPI::WalkPosition(targetPos)
                                   << ": goal=" << BWAPI::WalkPosition(scout.unit->lastPosition + BWAPI::Position(goalX, goalY))
                                   << "; threat=" << BWAPI::WalkPosition(scout.unit->lastPosition + BWAPI::Position(threatX, threatY))
                                   << "; total=" << BWAPI::WalkPosition(scout.unit->lastPosition + BWAPI::Position(goalX + threatX, goalY + threatY))
                                   << "; target=" << BWAPI::WalkPosition(pos);
#endif

    // Default to target pos if unit can't move in the desired direction
    if (pos == BWAPI::Positions::Invalid) pos = targetPos;

    scout.unit->moveTo(pos, true);
}

bool EarlyGameWorkerScout::selectScout()
{
    // Normally we scout after the first pylon
    // Exceptions are that we wait for the first gateway or forge if on a two-player map and enemy is not zerg,
    // and we wait for the nexus if we are doing a 12 nexus vs. Terran
    if (Map::getEnemyStartingMain()
        && (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss))
    {
        scout.unit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Gateway, 1);
        if (!scout.unit) scout.unit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Forge, 1);
    }
    else if (Strategist::isOurStrategy(PvT::OurStrategy::FastExpansion))
    {
        scout.unit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Nexus, 2);
    }
    else
    {
        scout.unit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Pylon, 1);
    }

    return scout.unit != nullptr;
}

void EarlyGameWorkerScout::selectSecondScout()
{
    // Only scout against Zerg or Random on a 4p map where we have 2 or more starting locations left to scout
    if (Map::getEnemyStartingMain()) return;
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss) return;
    if (BWAPI::Broodwar->getStartLocations().size() < 4) return;
    if (Map::unscoutedStartingLocations().size() < 2) return;

    // Always wait until the first scout is on its way
    if (!scout.reserved) return;

    // Scout after the first gateway or forge
    secondScout.unit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Gateway, 1);
    if (!secondScout.unit) secondScout.unit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Forge, 1);
}

BWAPI::TilePosition EarlyGameWorkerScout::getHighestPriorityScoutTile()
{
    generateTileScoutPriorities(scoutTiles, scoutAreas);

    int highestPriorityFrame = INT_MAX;
    int highestPriorityDist = INT_MAX;
    BWAPI::TilePosition highestPriority = BWAPI::TilePositions::Invalid;

    for (auto &priorityAndTiles : scoutTiles)
    {
        for (auto &tile : priorityAndTiles.second)
        {
            int desiredFrame = Map::lastSeen(tile) + priorityAndTiles.first;
            if (desiredFrame > highestPriorityFrame) continue;

            int dist = scout.unit->getDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16));

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
