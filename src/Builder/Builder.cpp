#include "Building.h"
#include "Workers.h"
#include "Units.h"
#include "PathFinding.h"
#include "Geo.h"
#include "BuildingPlacement.h"
#include "Map.h"
#include "Players.h"

#include "DebugFlag_UnitOrders.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_BOARD_VALUE true
#endif

namespace Constructor
{
    namespace
    {
        std::vector<std::shared_ptr<Building>> pendingBuildings;
        std::map<OwnedWorker, std::vector<std::shared_ptr<Building>>> builderQueues;
        std::set<OwnedWorker> reservedBuilders;

        /* Ensures the worker is being ordered to build this building */
        void build(Building &building)
        {
            if (building.gameUnit || !building.builder || !building.builder->exists()) return;

            building.addNoGoAreaWhenNeeded();

            /* If the builder is constructing, we have already given it the build command and are waiting */
            /* for the building to appear */
            if (building.builder->rawUnit->isConstructing()) return;

            /* If we are close to the build position and want to build now, issue the build command */
            int span = Geom::EdgeToEdgeDistance(building.builder->type, building.builder->prevPos, building.type, building.getPosition());
            if (span <= 24 && building.desiredStartFrame - frameNow <= BWAPI::Broodwar->getRemainingLatencyFrames())
            {
                /* Return immediately if issuing the build command succeeded */
                if (building.builder->build(building.type, building.tilePos))
                {
#if LOGGING_ENABLED
                    AppLog::Debug() << "Issued successful build command for builder " << building.builder->ident << " to build " << building;
#endif

                    building.buildCommandSuccessFrames++;
                    return;
                }

#if LOGGING_ENABLED
                AppLog::Debug() << "Builder " << building.builder->ident << " could not build " << building << ": " << BWAPI::Broodwar->getLastError();
#endif

                if (BWAPI::Broodwar->getLastError() != BWAPI::Errors::Insufficient_Minerals &&
                    BWAPI::Broodwar->getLastError() != BWAPI::Errors::Insufficient_Gas)
                {
                    building.buildCommandFailureFrames++;
                }
            }

            /* Move towards the build position */
#if DEBUG_UNIT_ORDERS
            Telemetry::log(building.builder->ident) << "moveTo: Build location " << BWAPI::WalkPosition(building.getPosition());
#endif
            building.builder->moveTo(building.getPosition());
        }

        /* Releases the builder from constructing the given building */
        /* The builder may still have more buildings in its queue */
        void releaseBuilder(Building &building)
        {
            if (!building.builder) return;

            auto builder = building.builder;
            building.builder = nullptr;

            /* Remove the building from the builder's queue */
            auto &builderQueue = builderQueues[builder];
            for (auto iterRef = builderQueue.begin(); iterRef != builderQueue.end();)
            {
                iterRef = iterRef->get() == &building ? builderQueue.erase(iterRef) : iterRef + 1;
            }

            /* When the builder has no more buildings in its queue, release it back to Workers, unless it is reserved */
            if (builderQueue.empty())
            {
                if (!reservedBuilders.contains(builder))
                {
                    Telemetry::log(builder->ident) << "Releasing from non-mining duties (builder queue empty)";
                    WorkerMgr::releaseWorker(builder);
                }

                builderQueues.erase(builder);
            }
        }

        void writeInstrumentation()
        {
#if CVIS_BOARD_VALUE
            std::vector<std::string> values;
            values.reserve(pendingBuildings.size());
            for (auto &pendingBuilding : pendingBuildings)
            {
                std::ostringstream ss;
                ss << pendingBuilding->type << " " << pendingBuilding->tilePos;
                if (pendingBuilding->builder)
                {
                    ss << " " << *pendingBuilding->builder;
                }
                values.emplace_back(ss.str());
            }
            Telemetry::setBoardListValue("builder", values);
#endif
        }
    }

    void initialize()
    {
        pendingBuildings.clear();
        builderQueues.clear();
    }

    void update()
    {
        /* Clear dead reserved builders */
        for (auto iterRef = reservedBuilders.begin(); iterRef != reservedBuilders.end();)
        {
            if ((*iterRef)->exists())
            {
                iterRef++;
            }
            else
            {
                iterRef = reservedBuilders.erase(iterRef);
            }
        }

        /* Prune the list of pending buildings */
        for (auto iterRef = pendingBuildings.begin(); iterRef != pendingBuildings.end();)
        {
            auto &building = **iterRef;

            /* Remove the building if: */
            /* - The building has completed */
            /* - The building has died while under construction */
            /* - The builder has died before starting construction */
            /* TODO: Cancel dying buildings */
            if ((building.gameUnit && (!building.gameUnit->exists() || building.gameUnit->completed))
                || (!building.gameUnit && !building.builder->exists()))
            {
                BuildingPlacement::onBuildingCancelled(&building);

                releaseBuilder(building);
                building.removeNoGoArea();
                iterRef = pendingBuildings.erase(iterRef);
                continue;
            }

            /* Check for buildings our worker is unable to build */
            if (!building.gameUnit)
            {
                /* If we've sent successful build commands for the past two seconds without the building starting, */
                /* assume the enemy is blocking it with something */
                if (building.buildCommandSuccessFrames >= 48)
                {
                    AppLog::Get() << "Build of " << building << " failed due to enemy blocking";

                    if (building.type == BWAPI::UnitTypes::Protoss_Nexus)
                    {
                        auto baseLoc = Map::baseNear(BWAPI::Position(building.tilePos));
                        if (baseLoc && !baseLoc->blockedByEnemy)
                        {
                            AppLog::Get() << "Base @ " << baseLoc->getTilePosition() << " is blocked by an enemy unit";
                            baseLoc->blockedByEnemy = true;
                        }
                    }
                }
                else if (building.buildCommandFailureFrames >= 240)
                {
                    AppLog::Get() << "Build of " << building << " failed due to own unit blocking or other error";
                }
                else
                {
                    iterRef++;
                    continue;
                }

                /* TODO: At some point we should restore the building placement, at least in some cases */
                releaseBuilder(building);
                building.removeNoGoArea();
                iterRef = pendingBuildings.erase(iterRef);
                continue;
            }

            iterRef++;
        }

        /* Check for started buildings */
        /* TODO: Flip this around and only check the unit list when we have sent the build order */
        if (!pendingBuildings.empty())
        {
            for (auto &gameUnit : UnitMgr::allMine())
            {
                if (!gameUnit->type.isBuilding() || gameUnit->completed) continue;
                for (auto &pendingBuilding : pendingBuildings)
                {
                    if (pendingBuilding->gameUnit) continue;
                    if (pendingBuilding->type != gameUnit->type) continue;
                    if (pendingBuilding->tilePos != gameUnit->getTilePosition()) continue;

                    pendingBuilding->constructionStarted(gameUnit);
                    releaseBuilder(*pendingBuilding);
                    pendingBuilding->removeNoGoArea();
                }
            }
        }

        /* Remove dead builders */
        /* The buildings themselves are pruned earlier */
        for (auto iterRef = builderQueues.begin(); iterRef != builderQueues.end();)
        {
            if (!iterRef->first->exists())
            {
                iterRef = builderQueues.erase(iterRef);
                continue;
            }

            iterRef++;
        }

        writeInstrumentation();
    }

    void issueOrders()
    {
        for (auto &builderQueue : builderQueues)
        {
            build(**builderQueue.second.begin());
        }
    }

    void build(BWAPI::UnitType type, BWAPI::TilePosition tilePos, const OwnedWorker &builder, int startFrame)
    {
        /* Check we don't already have a building here */
        auto existing = UnitMgr::myBuildingAt(tilePos);
        if (existing)
        {
            AppLog::Get() << "ERROR: Can't build " << type << " at " << tilePos << " as we already have " << existing->type;
            return;
        }

        /* Sanity check that we don't already have a pending building overlapping the tile */
        /* This happens if the producer orders two buildings on the same frame where one is using a build location converted from the other */
        /* When we detect this, we can just return; the producer will use a different build location on the next frame */
        for (auto &pendingBuilding : pendingBuildings)
        {
            if (pendingBuilding->isConstructionStarted()) continue;
            if (Geom::Overlaps(tilePos, type.tileWidth(), type.tileHeight(),
                              pendingBuilding->tilePos, pendingBuilding->type.tileWidth(), pendingBuilding->type.tileHeight()))
            {
                return;
            }
        }

        auto building = std::make_shared<Building>(type, tilePos, builder, startFrame);
        pendingBuildings.push_back(building);
        builderQueues[builder].push_back(building);

        WorkerMgr::reserveWorker(builder);

        AppLog::Debug() << "Queued " << **pendingBuildings.rbegin() << " to start at " << startFrame << " for builder " << builder->ident
                     << "; builder queue length: " << builderQueues[builder].size();

        BuildingPlacement::onBuildingQueued(building.get());
    }

    void cancel(BWAPI::TilePosition tilePos)
    {
        for (auto iterRef = pendingBuildings.begin(); iterRef != pendingBuildings.end(); iterRef++)
        {
            if ((*iterRef)->tilePos != tilePos) continue;

            /* If construction has started, cancel it */
            if ((*iterRef)->gameUnit)
            {
                AppLog::Get() << "Cancelling construction of " << *(*iterRef)->gameUnit;
                (*iterRef)->gameUnit->cancelConstruction();
            }

            (*iterRef)->removeNoGoArea();

            BuildingPlacement::onBuildingCancelled(iterRef->get());

            releaseBuilder(**iterRef);
            pendingBuildings.erase(iterRef);
            return;
        }
    }

    OwnedWorker getBuilderUnit(BWAPI::TilePosition tilePos, BWAPI::UnitType type, int *expectedArrivalFrame)
    {
        BWAPI::Position buildPosition = BWAPI::Position(tilePos) + BWAPI::Position(type.tileWidth() * 16, type.tileHeight() * 16);

        /* First get the closest worker currently available for reassignment */
        int bestMineralWorkerTravelTime = INT_MAX;
        OwnedWorker bestMineralWorker = WorkerMgr::getClosestReassignableWorker(buildPosition,
                                                                         !type.isResourceDepot(),
                                                                         &bestMineralWorkerTravelTime);

        /* Next get the best existing builder */
        OwnedWorker bestBuilder = nullptr;
        int bestBuilderTravelTime = INT_MAX;
        for (auto &builderAndQueue : builderQueues)
        {
            if (!builderAndQueue.first->exists()) continue;
            if (builderAndQueue.second.empty()) continue;

            int totalTravelTime = 0;

            /* Sum up the travel time between the existing queued buildings */
            BWAPI::Position prevPos = builderAndQueue.first->prevPos;
            for (const auto &building : builderAndQueue.second)
            {
                totalTravelTime +=
                        Pathfinder::ExpectedTravelTime(prevPos,
                                                        building->getPosition(),
                                                        builderAndQueue.first->type,
                                                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                prevPos = building->getPosition();
            }

            /* Add in the travel time to this next building */
            int expectedTravelTime = Pathfinder::ExpectedTravelTime(prevPos,
                                                                     buildPosition,
                                                                     builderAndQueue.first->type,
                                                                     Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                                     -1);
            if (expectedTravelTime == -1) continue; // Builder might be on an island
            totalTravelTime += expectedTravelTime;

            /* Give a bonus to already-building workers, as we don't want to take a lot of workers off minerals */
            if (totalTravelTime < bestBuilderTravelTime)
            {
                bestBuilderTravelTime = totalTravelTime;
                bestBuilder = builderAndQueue.first;
            }
        }

        /* Finally get the best reserved builder */
        OwnedWorker bestReserved = nullptr;
        int bestReservedTravelTime = INT_MAX;
        for (auto &reservedBuilder : reservedBuilders)
        {
            /* Don't consider this again if it already has a queue */
            if (builderQueues.contains(reservedBuilder)) continue;

            int expectedTravelTime = Pathfinder::ExpectedTravelTime(reservedBuilder->prevPos,
                                                                     buildPosition,
                                                                     reservedBuilder->type,
                                                                     Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                                     -1);
            if (expectedTravelTime == -1) continue; // Reserved builder might be on an island

            /* Give a bonus to reserved builders (similar to above) */
            if (expectedTravelTime < bestReservedTravelTime)
            {
                bestReservedTravelTime = expectedTravelTime;
                bestReserved = reservedBuilder;
            }
        }

        /* Pick the best worker, giving existing builders a 6-second bonus */
        if (bestReservedTravelTime < bestBuilderTravelTime)
        {
            bestBuilder = bestReserved;
            bestBuilderTravelTime = bestReservedTravelTime;
        }

        OwnedWorker builder;
        int travelTime;
        if (bestMineralWorkerTravelTime < (bestBuilderTravelTime - 144))
        {
            builder = bestMineralWorker;
            travelTime = bestMineralWorkerTravelTime;
        }
        else
        {
            builder = bestBuilder;
            travelTime = bestBuilderTravelTime;
        }

        if (expectedArrivalFrame)
            *expectedArrivalFrame = builder ? frameNow + travelTime : -1;
        return builder;
    }

    std::vector<std::shared_ptr<Building>> &allPendingBuildings()
    {
        return pendingBuildings;
    }

    std::vector<Building *> pendingBuildingsOfType(BWAPI::UnitType type)
    {
        std::vector<Building *> outcome;
        for (const auto &building : pendingBuildings)
        {
            if (building->type == type) outcome.push_back(building.get());
        }

        return outcome;
    }

    void cancelBase(Base *baseLoc)
    {
        cancel(baseLoc->getTilePosition());

        /* Also cancel assimilator if it is building */
        for (auto &pendingBuilding : pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Assimilator))
        {
            if (baseLoc->hasGeyserOrRefineryAt(pendingBuilding->tilePos))
            {
                cancel(pendingBuilding->tilePos);
            }
        }
    }

    bool isPendingHere(BWAPI::TilePosition tilePos)
    {
        return std::any_of(pendingBuildings.begin(), pendingBuildings.end(), [&tilePos](const auto &pendingBuilding)
        {
            return pendingBuilding->tilePos == tilePos;
        });
    }

    Building *pendingHere(BWAPI::TilePosition tilePos)
    {
        for (const auto &building : pendingBuildings)
        {
            if (building->tilePos == tilePos) return building.get();
        }

        return nullptr;
    }

    bool hasPendingBuilding(const OwnedWorker &builder)
    {
        auto iterRef = builderQueues.find(builder);
        if (iterRef == builderQueues.end()) return false;

        return !iterRef->second.empty();
    }

    void addReservedBuilder(const OwnedWorker &builder)
    {
        reservedBuilders.insert(builder);
    }

    void releaseReservedBuilder(const OwnedWorker &builder)
    {
        reservedBuilders.erase(builder);
    }

    bool isInEnemyStaticThreatRange(BWAPI::TilePosition tilePos, BWAPI::UnitType type)
    {
        auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());

        if (grid.staticGroundThreat(BWAPI::WalkPosition(tilePos)) > 0) return true;
        if (grid.staticGroundThreat(BWAPI::WalkPosition(tilePos + BWAPI::TilePosition(type.tileWidth(), 0))) > 0) return true;
        if (grid.staticGroundThreat(BWAPI::WalkPosition(tilePos + BWAPI::TilePosition(type.tileWidth(), type.tileHeight()))) > 0) return true;
        if (grid.staticGroundThreat(BWAPI::WalkPosition(tilePos + BWAPI::TilePosition(0, type.tileHeight()))) > 0) return true;

        return false;
    }

    int framesUntilCompleted(BWAPI::TilePosition tilePos, int defaultValue)
    {
        auto gameUnit = UnitMgr::myBuildingAt(tilePos);
        if (gameUnit)
        {
            if (gameUnit->completed) return 0;
            return gameUnit->estimatedCompletionFrame - frameNow;
        }

        auto pendingBuilding = Constructor::pendingHere(tilePos);
        if (pendingBuilding)
        {
            return pendingBuilding->expectedFramesUntilCompletion();
        }

        return defaultValue;
    }
}
