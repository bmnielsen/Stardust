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

    OwnedWorker getScoutAfterBuilding(BWAPI::UnitType buildingType, int buildingCount, const OwnedWorker &existingScout = nullptr)
    {
        /* If a building of the required type already exists, this play is being added late */
        /* This happens vs. random when the play is re-initialized once the enemy race is known */
        /* In this case grab our furthest worker from the main base, as it is likely to have already been our scout */
        if (UnitMgr::countAll(buildingType) >= buildingCount)
        {
            OwnedWorker bestWorker = nullptr;
            int bestDist = 0;
            for (const auto &gameUnit : UnitMgr::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                auto wrk = std::static_pointer_cast<MyWorkerImpl>(gameUnit);
                if (existingScout && existingScout->ident == wrk->ident) continue;

                int span = Pathfinder::GetGroundDistance(wrk->prevPos,
                                                          Map::getMyMain()->getPosition(),
                                                          BWAPI::UnitTypes::Protoss_Probe,
                                                          Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                if (span > bestDist)
                {
                    bestDist = span;
                    bestWorker = wrk;
                }
            }

            return bestWorker;
        }

        for (auto &pendingBuilding : Constructor::allPendingBuildings())
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
        /* Reserve the scout if it hasn't already been done */
        if (!scout.reserved)
        {
            /* Wait for the worker to be available for reassignment */
            if (!WorkerMgr::isAvailableForReassignment(scout.gameUnit, false, false)) return;

            /* Reserve the worker */
            WorkerMgr::reserveWorker(scout.gameUnit);
            scout.reserved = true;
        }

        /* Jump out if this scout is already assigned to a valid base */
        if (scout.targetBase && scout.targetBase == Map::getEnemyStartingMain()) return;
        if (scout.targetBase && !scout.targetBase->owner && scout.targetBase->lastScouted == -1) return;

        /* Reset before picking a new base */
        scout.targetBase = nullptr;
        scout.closestDistanceToTargetBase = INT_MAX;
        scout.lastDistanceToTargetBase = INT_MAX;
        scout.lastForwardMotionFrame = frameNow;

        /* Pick the closest remaining base to the worker that isn't assigned to another scout */
        /* TODO: On four player maps with buildable centers, it may be a good idea to cross the center to uncover proxies */
        Base *bestBase = nullptr;
        int bestTravelTime = INT_MAX;
        for (auto baseLoc : Map::unscoutedStartingLocations())
        {
            if (otherScout.targetBase == baseLoc) continue;

            int travelTime =
                    Pathfinder::ExpectedTravelTime(scout.gameUnit->prevPos,
                                                    baseLoc->getPosition(),
                                                    scout.gameUnit->type,
                                                    Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                    1.1,
                                                    -1);
            if (travelTime != -1 && travelTime < bestTravelTime)
            {
                bestTravelTime = travelTime;
                bestBase = baseLoc;
            }
        }

        /* Move towards the base */
        if (bestBase)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(scout.gameUnit->ident) << "moveTo: Scout base @ " << BWAPI::WalkPosition(bestBase->getPosition());
#endif
            scout.gameUnit->moveTo(bestBase->getPosition());
            scout.targetBase = bestBase;
            if (bestBase == Map::getEnemyStartingMain())
            {
                Director::setWorkerScoutStatus(Director::WorkerScoutStatus::MovingToEnemyBase);
            }
            else
            {
                Director::setWorkerScoutStatus(Director::WorkerScoutStatus::LookingForEnemyBase);
            }
        }
    }

    bool isScoutBlocked(EarlyGameWorkerScout::ScoutLookingForEnemyBase &scout)
    {
        if (!scout.gameUnit || !scout.reserved || !scout.targetBase) return false;

        /* Not blocked if we got into the target base once */
        /* TODO: May want to detect blocking after we've been out to check the natural at some point, but for now we just handle initial blocking */
        if (scout.targetBase->lastScouted != -1) return false;

        /* Get cost from here to the target base */
        auto grid = Pathfinder::getNavigationGrid(scout.targetBase->getPosition());
        if (!grid) return false;
        auto node = (*grid)[scout.gameUnit->prevPos];
        if (node.cost == USHRT_MAX)
        {
            /* This means either that our scout is in an unexpected position or the enemy has done a wall-in */
            /* Check for the latter by checking if there is a path from the target base to our main */
            auto mainGrid = Pathfinder::getNavigationGrid(Map::getMyMain()->getPosition());
            if (!mainGrid) return false;

            auto targetNode = (*mainGrid)[scout.targetBase->getTilePosition()];
            if (targetNode.cost == USHRT_MAX)
            {
                Map::setEnemyStartingMain(scout.targetBase);
                return true;
            }

            return false;
        }

        /* Decreasing distance is fine */
        /* Increasing distance is fine if it jumps quite a bit - this generally means we've scouted a building that changes the path */
        if (node.cost<scout.closestDistanceToTargetBase || node.cost>(scout.lastDistanceToTargetBase + 100))
        {
            scout.closestDistanceToTargetBase = node.cost;
            scout.lastDistanceToTargetBase = node.cost;
            scout.lastForwardMotionFrame = frameNow;
            return false;
        }

        scout.lastDistanceToTargetBase = node.cost;

        /* Non-decreasing distance is fine if we are still far away from the enemy base */
        if (node.cost > 3000) return false;

        /* Consider us to be blocked if we haven't made forward progress in five seconds */
        if ((frameNow - scout.lastForwardMotionFrame) > 120)
        {
            Map::setEnemyStartingMain(scout.targetBase);
            return true;
        }

        return false;
    }

    /* Once we know the enemy main base, this method generates the map of prioritized tiles to scout */
    /* Top priority are mineral patches, geysers, and the area immediately around the resource depot */
    /* Next priority is the natural location */
    /* Lowest priority is all other tiles in the base area, unless the enemy is zerg (since they can only build on creep anyway) */
    void generateTileScoutPriorities(std::map<int, std::set<BWAPI::TilePosition>> &scoutTiles,
                                     std::set<const BWEM::Area *> &scoutAreas)
    {
        auto baseLoc = Map::getEnemyStartingMain();
        if (!baseLoc) return;

        scoutTiles.clear();

        /* Determine the priorities to use */
        int areaPriority = 800;
        int naturalPriority = 960;

        /* For Zerg we don't need to scout around the base (as they can only build on creep), but want to scout the natural more often */
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
        {
            areaPriority = 0;
            naturalPriority = 600;
        }

        /* If we suspect a proxy, scout the natural and outskirts more aggressively early on in case we missed something */
        if (Director::getStrategyEngine()->isEnemyProxy() && frameNow < 4000)
        {
            naturalPriority = 480;
            areaPriority = 600;
        }

        /* If the enemy is rushing, give the main area and natural much lower priority */
        /* We mainly want to keep an eye on when they start transitioning out of the rush */
        else if (Director::getStrategyEngine()->isEnemyRushing() && BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg)
        {
            areaPriority = 1200;
            naturalPriority = 1200;
        }

        /* If the enemy is protoss, don't scout the natural if our economic model tells us they can't have taken an extra nexus yet */
        else if (OpponentEconomicModel::enabled() &&
            OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Nexus) > frameNow)
        {
            naturalPriority = 0;
        }

        auto tileValid = [](BWAPI::TilePosition tilePos, int neutralElevation)
        {
            if (!tilePos.isValid()) return false;
            if (!BWAPI::Broodwar->isBuildable(tilePos)) return false;
            if (BWAPI::Broodwar->getGroundHeight(tilePos) > neutralElevation) return false;

            return true;
        };

        int baseElevation = BWAPI::Broodwar->getGroundHeight(baseLoc->getTilePosition());

        /* Start by assigning all tiles in the base area the lowest priority */
        if (areaPriority)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    BWAPI::TilePosition here(x, y);
                    if (!tileValid(here, baseElevation)) continue;
                    if (BWEM::Map::Instance().GetArea(here) != baseLoc->getArea()) continue;

                    scoutTiles[areaPriority].insert(here);
                }
            }
        }

        /* Now add tiles close to the natural */
        auto natBase = Map::getEnemyStartingNatural();
        if (natBase && naturalPriority)
        {
            int naturalElevation = BWAPI::Broodwar->getGroundHeight(natBase->getTilePosition());
            for (int x = -3; x < 7; x++)
            {
                for (int y = -3; y < 6; y++)
                {
                    auto here = natBase->getTilePosition() + BWAPI::TilePosition(x, y);

                    if ((BWAPI::Position(here) + BWAPI::Position(16, 16)).getApproxDistance(natBase->getPosition()) > 160) continue;

                    if (!tileValid(here, naturalElevation)) continue;

                    scoutTiles[naturalPriority].insert(here);
                }
            }
        }

        /* Now add the tiles close to the depot, except for the mineral line */
        for (int x = -6; x < 10; x++)
        {
            for (int y = -6; y < 9; y++)
            {
                auto here = baseLoc->getTilePosition() + BWAPI::TilePosition(x, y);

                if ((BWAPI::Position(here) + BWAPI::Position(16, 16)).getApproxDistance(baseLoc->getPosition()) > 250) continue;

                if (!tileValid(here, baseElevation)) continue;

                if (baseLoc->isInMineralLine(here)) continue;

                scoutTiles[mainPriority].insert(here);
            }
        }

        /* Finally generate the areas covered by the scout */
        scoutAreas.insert(baseLoc->getArea());
        if (natBase)
        {
            auto path = Pathfinder::GetChokePointPath(baseLoc->getPosition(), natBase->getPosition());
            for (auto choke : path)
            {
                scoutAreas.insert(choke->GetAreas().first);
                scoutAreas.insert(choke->GetAreas().second);
            }
        }

#if OUTPUT_SCOUTTILE_HEATMAP
        std::vector<long> scoutTilesCvis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (auto iterRef = scoutTiles.rbegin(); iterRef != scoutTiles.rend(); iterRef++)
        {
            for (auto &tilePos : iterRef->second)
            {
                scoutTilesCvis[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] = iterRef->first;
            }
        }
        if (lastScoutTilesCvis != scoutTilesCvis)
        {
            Telemetry::addHeatmap("WorkerScoutTiles", scoutTilesCvis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
        }
        lastScoutTilesCvis = scoutTilesCvis;
#endif
    }

    BWAPI::TilePosition getTileToHideOn()
    {
        auto enemyMain = Map::getEnemyStartingMain();
        if (!enemyMain) return BWAPI::TilePositions::Invalid;

        /* Collect all of the tiles the enemy has vision on */
        std::set<BWAPI::TilePosition> visionTiles;
        for (const auto &gameUnit : UnitMgr::allEnemy())
        {
            if (!gameUnit->type.isBuilding()) continue;

            auto cornerTL = gameUnit->getTilePosition();

            int tileSightRadius = (gameUnit->type.sightRange() + 31) / 32;
            for (int x = -tileSightRadius; x < gameUnit->type.tileWidth() + tileSightRadius; x++)
            {
                for (int y = -tileSightRadius; y < gameUnit->type.tileHeight() + tileSightRadius; y++)
                {
                    auto here = cornerTL + BWAPI::TilePosition(x, y);
                    if (!here.isValid()) continue;

                    if (Geom::EdgeToTileDistance(gameUnit->type, cornerTL, here) <= tileSightRadius)
                    {
                        visionTiles.insert(here);
                    }
                }
            }
        }

        /* Now get the best tile in the main area */
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

                int span = enemyMain->getPosition().getApproxDistance(BWAPI::Position(here) + BWAPI::Position(16, 16));
                if (enemyChoke)
                {
                    span += enemyChoke->midpoint.getApproxDistance(BWAPI::Position(here) + BWAPI::Position(16, 16));
                }

                if (span > bestDist)
                {
                    bestDist = span;
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

        /* We measure potential tiles against the natural choke by default, or our main if a natural choke doesn't exist */
        auto referencePosition = Map::getMyMain()->getPosition();
        auto chokePath = Pathfinder::GetChokePointPath(referencePosition,
                                                        enemyMainChoke->midpoint,
                                                        BWAPI::UnitTypes::Protoss_Dragoon,
                                                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);
        if (!chokePath.empty())
        {
            auto iterRef = chokePath.rbegin();
            auto naturalChoke = Map::choke(*iterRef);

            /* Depending on the choke geography we might get the choke itself as the last part of the path */
            /* If so, advance to the next one */
            if (naturalChoke == enemyMainChoke)
            {
                iterRef++;
                if (iterRef != chokePath.rend()) naturalChoke = Map::choke(*iterRef);
            }

            if (naturalChoke)
            {
                referencePosition = naturalChoke->midpoint;
            }
        }

        /* Use our natural's height as the reference height */
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
                auto here = BWAPI::TilePosition(enemyMainChoke->midpoint) + BWAPI::TilePosition(x, y);
                if (!here.isValid()) continue;
                if (!Map::isWalkable(here)) continue;

                if (referenceHeight != -1 && BWAPI::Broodwar->getGroundHeight(here) != referenceHeight) continue;

                int chokeDist = enemyMainChoke->midpoint.getApproxDistance(BWAPI::Position(here) + BWAPI::Position(16, 16));
                if (chokeDist > BWAPI::UnitTypes::Protoss_Probe.sightRange()) continue;

                int span = referencePosition.getApproxDistance(BWAPI::Position(here) + BWAPI::Position(16, 16));
                if (span < bestDist)
                {
                    bestDist = span;
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


void EarlyGameWorkerScout::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                                   const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    auto releaseScout = [](const EarlyGameWorkerScout::ScoutLookingForEnemyBase &s)
    {
        if (s.gameUnit && s.gameUnit->exists())
        {
            Telemetry::log(s.gameUnit->ident) << "Releasing from non-mining duties (scout disband)";
            WorkerMgr::releaseWorker(s.gameUnit);
        }
    };
    releaseScout(scout);
    releaseScout(secondScout);

    if (Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::EnemyBaseScouted ||
        Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::MonitoringEnemyChoke)
    {
        Director::setWorkerScoutStatus(Director::WorkerScoutStatus::ScoutingCompleted);
    }
}

void EarlyGameWorkerScout::hideUntil(int tick)
{
#if CHERRYVIS_ENABLED
    if (tick != hidingUntil)
    {
        Telemetry::log() << "Hiding worker scout until frame " << tick;
    }
#endif

    hidingUntil = tick;
}

void EarlyGameWorkerScout::monitorEnemyChoke()
{
    fixedPosition = getTileToMonitorChokeFrom();
}

void EarlyGameWorkerScout::findEnemyBase()
{
    /* Handle initial state where we haven't reserved the worker scout yet */
    if (!scout.gameUnit && !selectScout()) return;

    if (!secondScout.gameUnit) selectSecondScout();

    /* Mark the play completed if one of the scouts die */
    if (!scout.gameUnit->exists() || (secondScout.gameUnit && !secondScout.gameUnit->exists()))
    {
        Director::setWorkerScoutStatus(Director::WorkerScoutStatus::ScoutingFailed);
        status.complete = true;
        return;
    }

    /* Update the target base */
    updateTargetBase(scout, secondScout);
    updateTargetBase(secondScout, scout);

    /* Handle case where no base can be scouted */
    /* This indicates an island map */
    if (scout.reserved && !scout.targetBase && (!secondScout.reserved || !secondScout.targetBase))
    {
        Director::setWorkerScoutStatus(Director::WorkerScoutStatus::ScoutingFailed);

        status.complete = true;
        return;
    }

    /* Handle case where one scout has nothing left to do */
    if (scout.reserved && !scout.targetBase)
    {
        /* Second scout takes over for the first */
        Telemetry::log(scout.gameUnit->ident) << "Releasing from non-mining duties (other scout takes over)";
        WorkerMgr::releaseWorker(scout.gameUnit);

        scout.gameUnit = secondScout.gameUnit;
        scout.targetBase = secondScout.targetBase;
        scout.closestDistanceToTargetBase = secondScout.closestDistanceToTargetBase;
        scout.lastDistanceToTargetBase = secondScout.lastDistanceToTargetBase;
        scout.lastForwardMotionFrame = secondScout.lastForwardMotionFrame;

        secondScout.gameUnit = nullptr;
        secondScout.reserved = false;
        secondScout.targetBase = nullptr;
    }
    if (secondScout.reserved && !secondScout.targetBase)
    {
        /* Second scout is no longer needed */
        Telemetry::log(secondScout.gameUnit->ident) << "Releasing from non-mining duties (other scout takes over)";
        WorkerMgr::releaseWorker(secondScout.gameUnit);

        secondScout.gameUnit = nullptr;
        secondScout.reserved = false;
        secondScout.targetBase = nullptr;
    }

    /* Detect if the enemy is blocking our scout from getting into the target base */
    if (isScoutBlocked(scout) || isScoutBlocked(secondScout))
    {
        Director::setWorkerScoutStatus(Director::WorkerScoutStatus::ScoutingBlocked);

        status.complete = true;
        return;
    }
}

void EarlyGameWorkerScout::scoutEnemyBase()
{
    /* If we still have two scouts, release one */
    if (secondScout.gameUnit)
    {
        if (!secondScout.reserved || !secondScout.gameUnit->exists())
        {
            secondScout.gameUnit = nullptr;
        }
        else
        {
            /* Swap if the second scout found the base */
            if (!scout.gameUnit || !scout.gameUnit->exists() ||
                secondScout.gameUnit->getDistance(Map::getEnemyStartingMain()->getPosition()) <
                scout.gameUnit->getDistance(Map::getEnemyStartingMain()->getPosition()))
            {
                if (scout.gameUnit && scout.gameUnit->exists())
                {
                    Telemetry::log(scout.gameUnit->ident) << "Releasing from non-mining duties (other scout takes over)";
                    WorkerMgr::releaseWorker(scout.gameUnit);
                }

                scout.gameUnit = secondScout.gameUnit;
                scout.closestDistanceToTargetBase = secondScout.closestDistanceToTargetBase;
                scout.lastDistanceToTargetBase = secondScout.lastDistanceToTargetBase;
                scout.lastForwardMotionFrame = secondScout.lastForwardMotionFrame;

                secondScout.gameUnit = nullptr;
                secondScout.reserved = false;
                secondScout.targetBase = nullptr;
            }
            else
            {
                /* Second scout is no longer needed */
                Telemetry::log(secondScout.gameUnit->ident) << "Releasing from non-mining duties (other scout takes over)";
                WorkerMgr::releaseWorker(secondScout.gameUnit);

                secondScout.gameUnit = nullptr;
                secondScout.reserved = false;
                secondScout.targetBase = nullptr;
            }
        }
    }

    /* Mark the play completed if the scout dies */
    if (!scout.gameUnit || !scout.gameUnit->exists())
    {
        if (Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::EnemyBaseScouted ||
            Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::MonitoringEnemyChoke)
        {
            Director::setWorkerScoutStatus(Director::WorkerScoutStatus::ScoutingCompleted);
        }
        else
        {
            Director::setWorkerScoutStatus(Director::WorkerScoutStatus::ScoutingFailed);
        }

        status.complete = true;
        return;
    }

    scout.targetBase = Map::getEnemyStartingMain();

    /* Determine when the scout has seen most of the highest-priority tiles */
    if (Director::getWorkerScoutStatus() != Director::WorkerScoutStatus::EnemyBaseScouted &&
        Director::getWorkerScoutStatus() != Director::WorkerScoutStatus::MonitoringEnemyChoke &&
        !scoutTiles.empty())
    {
        int seen = 0;
        for (auto &mainTile : scoutTiles.begin()->second)
        {
            if (Map::lastSeen(mainTile) > 0) seen++;
        }

        if ((double) seen / (double) scoutTiles.begin()->second.size() > 0.8)
        {
            Director::setWorkerScoutStatus(Director::WorkerScoutStatus::EnemyBaseScouted);
        }
    }

    /* Determine the next tile we want to scout or move to */
    BWAPI::TilePosition tilePos;
    if (hidingUntil > frameNow)
    {
        tilePos = getTileToHideOn();
    }
    else if (fixedPosition.isValid())
    {
        tilePos = fixedPosition;
        if (scout.gameUnit->getDistance(BWAPI::Position(tilePos) + BWAPI::Position(16, 16)) < 64)
        {
            Director::setWorkerScoutStatus(Director::WorkerScoutStatus::MonitoringEnemyChoke);
        }
        else
        {
            Director::setWorkerScoutStatus(Director::WorkerScoutStatus::EnemyBaseScouted);
        }
    }
    else
    {
        tilePos = getHighestPriorityScoutTile();
    }
    if (!tilePos.isValid()) tilePos = BWAPI::TilePosition(scout.targetBase->getPosition());

    /* Compute threat avoidance boid */
    int threatX = 0;
    int threatY = 0;
    bool hasThreat = false;
    bool hasRangedThreat = false;
    for (auto &gameUnit : UnitMgr::allEnemy())
    {
        if (!gameUnit->lastPositionValid) continue;
        if (!UnitHelper::IsCombatUnit(gameUnit->type) && gameUnit->lastSeenAttacking < (frameNow - 120)) continue;
        if (!UnitHelper::CanAttackGround(gameUnit->type)) continue;
        if (!gameUnit->type.isBuilding() && gameUnit->lastSeen < (frameNow - 120)) continue;
        if (!gameUnit->completed) continue;

        int detectionLimit = std::max(128, gameUnit->groundRange() + 32);
        int span = scout.gameUnit->getDistance(gameUnit);
        if (span >= detectionLimit) continue;

        hasThreat = true;

        /* If the enemy has a ranged unit inside our detection limit, skip threat avaoidance entirely */
        /* Rationale is that we can't get away anyway, so let's just get the scouting done that we can before dying */
        if (UnitHelper::IsRangedUnit(gameUnit->type))
        {
            hasRangedThreat = true;
            break;
        }

        /* Minimum force at detection limit, maximum force at detection limit - 32 (and closer) */
        double distFactor = 1.0 - (double) std::max(0, span - 32) / (double) (detectionLimit - 32);
        auto vector = BWAPI::Position(scout.gameUnit->prevPos.x - gameUnit->prevPos.x, scout.gameUnit->prevPos.y - gameUnit->prevPos.y);
        auto scaled = Geom::ScaleVector(vector, (int) (distFactor * threatWeight));

        threatX += scaled.x;
        threatY += scaled.y;
    }

    /* If we have scouted the base at least once and the enemy has a ranged unit, complete the play */
    /* Direction 2 (2026-09-24): suppressed while the engine holds the DT-scout */
    /* window open (ignoreRangedThreatCompletion) - see the updatePlays block. */
    if (hasRangedThreat && !ignoreRangedThreatCompletion
        && Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::EnemyBaseScouted)
    {
        status.complete = true;
        return;
    }

    /* Get the next waypoint */
    BWAPI::Position targetPos;

    /* If we are outside the scout areas, use the navigation grid */
    if (!scoutAreas.contains(BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(scout.gameUnit->prevPos))))
    {
        /* Move directly if there is no enemy threat or a ranged threat */
        if (!hasThreat || hasRangedThreat)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(scout.gameUnit->ident) << "Scout: out of scout areas, move directly to scout tile " << BWAPI::WalkPosition(tilePos);
#endif
            scout.gameUnit->moveTo(BWAPI::Position(tilePos) + BWAPI::Position(16, 16));
            return;
        }

        auto navigationGrid = Pathfinder::getNavigationGrid(scout.targetBase->getPosition());
        auto node = navigationGrid ? &(*navigationGrid)[scout.gameUnit->getTilePosition()] : nullptr;
        node = node ? node->nextNode : nullptr;
        node = node ? node->nextNode : nullptr;
        if (!node)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(scout.gameUnit->ident) << "Scout: out of scout areas and no valid navigation grid node, move directly to scout tile "
                                      << BWAPI::WalkPosition(tilePos);
#endif
            scout.gameUnit->moveTo(BWAPI::Position(tilePos) + BWAPI::Position(16, 16));
            return;
        }

        targetPos = node->midpoint();
    }
    else
    {
        /* Plot a path, avoiding static defenses and the enemy mineral line */
        /* Also reject tiles outside the scout areas to limit the search space */
        auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());
        auto avoidThreatTiles = [&](BWAPI::TilePosition tilePos)
        {
            if (!Map::isWalkable(tilePos)) return false;
            if (!scoutAreas.contains(BWEM::Map::Instance().GetNearestArea(tilePos)))
            {
                return false;
            }
            if (scout.targetBase->isInMineralLine(tilePos)) return false;

            auto walk = BWAPI::WalkPosition(tilePos);
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
            return here.getApproxDistance(tilePos) < 6 && BWAPI::Broodwar->getGroundHeight(here) >= BWAPI::Broodwar->getGroundHeight(tilePos);
        };
        auto path = Pathfinder::Search(scout.gameUnit->getTilePosition(), tilePos, avoidThreatTiles, closeEnoughToTarget);

        /* Choose the appropriate target position */
        if (path.size() < 3)
        {
            targetPos = BWAPI::Position(tilePos) + BWAPI::Position(16, 16);

#if DEBUG_UNIT_ORDERS
            Telemetry::log(scout.gameUnit->ident) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
        }
        else
        {
            targetPos = BWAPI::Position(path[2]) + BWAPI::Position(16, 16);

#if DEBUG_UNIT_ORDERS
            Telemetry::log(scout.gameUnit->ident) << "Scout: target next path waypoint " << BWAPI::WalkPosition(targetPos);
#endif
        }
    }

    /* If there is a ranged threat or no threats, move directly */
    if (!hasThreat || hasRangedThreat)
    {
#if DEBUG_UNIT_ORDERS
        if (hasRangedThreat)
        {
            Telemetry::log(scout.gameUnit->ident) << "Scout: Ranged threat, moving directly to target " << BWAPI::WalkPosition(targetPos);
        }
        else
        {
            Telemetry::log(scout.gameUnit->ident) << "Scout: No threats, moving directly to target " << BWAPI::WalkPosition(targetPos);
        }
#endif

        scout.gameUnit->moveTo(targetPos, true);
        return;
    }

    /* Compute goal boid */
    int goalX = 0;
    int goalY = 0;
    {
        auto vector = BWAPI::Position(targetPos.x - scout.gameUnit->prevPos.x, targetPos.y - scout.gameUnit->prevPos.y);
        auto scaled = Geom::ScaleVector(vector, goalWeight);
        if (scaled != BWAPI::Positions::Invalid)
        {
            goalX = scaled.x;
            goalY = scaled.y;
        }
    }

    auto pt = Boids::ComputePosition(scout.gameUnit.get(), {goalX, threatX}, {goalY, threatY}, 96, UnitHelper::HaltDistance(scout.gameUnit->type) + 16, true);

#if DEBUG_UNIT_BOIDS
    Telemetry::log(scout.gameUnit->ident) << "Scouting boids towards " << BWAPI::WalkPosition(targetPos)
                                   << ": goal=" << BWAPI::WalkPosition(scout.gameUnit->prevPos + BWAPI::Position(goalX, goalY))
                                   << "; threat=" << BWAPI::WalkPosition(scout.gameUnit->prevPos + BWAPI::Position(threatX, threatY))
                                   << "; total=" << BWAPI::WalkPosition(scout.gameUnit->prevPos + BWAPI::Position(goalX + threatX, goalY + threatY))
                                   << "; target=" << BWAPI::WalkPosition(pt);
#endif

    /* Default to target pos if unit can't move in the desired direction */
    if (pt == BWAPI::Positions::Invalid) pt = targetPos;

    scout.gameUnit->moveTo(pt, true);
}

bool EarlyGameWorkerScout::selectScout()
{
    /* Normally we scout after the first pylon */
    /* Exceptions are that we wait for the first gateway or forge if on a two-player map and enemy is not zerg, */
    /* and we wait for the nexus if we are doing a 12 nexus vs. Terran */
    if (Map::getEnemyStartingMain()
        && (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss))
    {
        scout.gameUnit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Gateway, 1);
        if (!scout.gameUnit) scout.gameUnit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Forge, 1);
    }
    else if (Director::isOurStrategy(PvT::OurStrategy::FastExpansion))
    {
        scout.gameUnit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Nexus, 2);
    }
    else
    {
        scout.gameUnit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Pylon, 1);
    }

    return scout.gameUnit != nullptr;
}

void EarlyGameWorkerScout::selectSecondScout()
{
    /* Only scout against Zerg or Random on a 4p map where we have 2 or more starting locations left to scout */
    if (Map::getEnemyStartingMain()) return;
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss) return;
    if (BWAPI::Broodwar->getStartLocations().size() < 4) return;
    if (Map::unscoutedStartingLocations().size() < 2) return;

    /* Always wait until the first scout is on its way */
    if (!scout.reserved) return;

    /* Scout after the first gateway or forge */
    secondScout.gameUnit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Gateway, 1);
    if (!secondScout.gameUnit) secondScout.gameUnit = getScoutAfterBuilding(BWAPI::UnitTypes::Protoss_Forge, 1);
}

BWAPI::TilePosition EarlyGameWorkerScout::getHighestPriorityScoutTile()
{
    generateTileScoutPriorities(scoutTiles, scoutAreas);

    int highestPriorityFrame = INT_MAX;
    int highestPriorityDist = INT_MAX;
    BWAPI::TilePosition highestPriority = BWAPI::TilePositions::Invalid;

    for (auto &priorityAndTiles : scoutTiles)
    {
        for (auto &tilePos : priorityAndTiles.second)
        {
            int desiredFrame = Map::lastSeen(tilePos) + priorityAndTiles.first;
            if (desiredFrame > highestPriorityFrame) continue;

            int span = scout.gameUnit->getDistance(BWAPI::Position(tilePos) + BWAPI::Position(16, 16));

            if (desiredFrame < highestPriorityFrame || span < highestPriorityDist)
            {
                highestPriorityFrame = desiredFrame;
                highestPriorityDist = span;
                highestPriority = tilePos;
            }
        }
    }

    return highestPriority;
}
