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

namespace Builder
{
    namespace
    {
        std::vector<std::shared_ptr<Building>> pendingBuildings;
        std::map<MyUnit, std::vector<std::shared_ptr<Building>>> builderQueues;
        std::set<MyUnit> reservedBuilders;

        // Ensures the worker is being ordered to build this building
        void build(Building &building)
        {
            if (building.unit || !building.builder || !building.builder->exists()) return;

            building.addNoGoAreaWhenNeeded();

            // If the builder is constructing, we have already given it the build command and are waiting
            // for the building to appear
            if (building.builder->bwapiUnit->isConstructing()) return;

            // If we are close to the build position and want to build now, issue the build command
            int dist = Geo::EdgeToEdgeDistance(building.builder->type, building.builder->lastPosition, building.type, building.getPosition());
            if (dist <= 24 && building.desiredStartFrame - currentFrame <= BWAPI::Broodwar->getRemainingLatencyFrames())
            {
                // Return immediately if issuing the build command succeeded
                if (building.builder->build(building.type, building.tile))
                {
#if LOGGING_ENABLED
                    Log::Debug() << "Issued successful build command for builder " << building.builder->id << " to build " << building;
#endif

                    building.buildCommandSuccessFrames++;
                    return;
                }

#if LOGGING_ENABLED
                Log::Debug() << "Builder " << building.builder->id << " could not build " << building << ": " << BWAPI::Broodwar->getLastError();
#endif

                if (BWAPI::Broodwar->getLastError() != BWAPI::Errors::Insufficient_Minerals &&
                    BWAPI::Broodwar->getLastError() != BWAPI::Errors::Insufficient_Gas)
                {
                    building.buildCommandFailureFrames++;
                }
            }

            // Move towards the build position
#if DEBUG_UNIT_ORDERS
            CherryVis::log(building.builder->id) << "moveTo: Build location " << BWAPI::WalkPosition(building.getPosition());
#endif
            building.builder->moveTo(building.getPosition());
        }

        // Releases the builder from constructing the given building
        // The builder may still have more buildings in its queue
        void releaseBuilder(Building &building)
        {
            if (!building.builder) return;

            auto builder = building.builder;
            building.builder = nullptr;

            // Remove the building from the builder's queue
            auto &builderQueue = builderQueues[builder];
            for (auto it = builderQueue.begin(); it != builderQueue.end();)
            {
                it = it->get() == &building ? builderQueue.erase(it) : it + 1;
            }

            // When the builder has no more buildings in its queue, release it back to Workers, unless it is reserved
            if (builderQueue.empty())
            {
                if (!reservedBuilders.contains(builder))
                {
                    CherryVis::log(builder->id) << "Releasing from non-mining duties (builder queue empty)";
                    Workers::releaseWorker(builder);
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
                ss << pendingBuilding->type << " " << pendingBuilding->tile;
                if (pendingBuilding->builder)
                {
                    ss << " " << *pendingBuilding->builder;
                }
                values.emplace_back(ss.str());
            }
            CherryVis::setBoardListValue("builder", values);
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
        // Clear dead reserved builders
        for (auto it = reservedBuilders.begin(); it != reservedBuilders.end();)
        {
            if ((*it)->exists())
            {
                it++;
            }
            else
            {
                it = reservedBuilders.erase(it);
            }
        }

        // Prune the list of pending buildings
        for (auto it = pendingBuildings.begin(); it != pendingBuildings.end();)
        {
            auto &building = **it;

            // Remove the building if:
            // - The building has completed
            // - The building has died while under construction
            // - The builder has died before starting construction
            // TODO: Cancel dying buildings
            if ((building.unit && (!building.unit->exists() || building.unit->completed))
                || (!building.unit && !building.builder->exists()))
            {
                BuildingPlacement::onBuildingCancelled(&building);

                releaseBuilder(building);
                building.removeNoGoArea();
                it = pendingBuildings.erase(it);
                continue;
            }

            // Check for buildings our worker is unable to build
            if (!building.unit)
            {
                // If we've sent successful build commands for the past two seconds without the building starting,
                // assume the enemy is blocking it with something
                if (building.buildCommandSuccessFrames >= 48)
                {
                    Log::Get() << "Build of " << building << " failed due to enemy blocking";

                    if (building.type == BWAPI::UnitTypes::Protoss_Nexus)
                    {
                        auto base = Map::baseNear(BWAPI::Position(building.tile));
                        if (base && !base->blockedByEnemy)
                        {
                            Log::Get() << "Base @ " << base->getTilePosition() << " is blocked by an enemy unit";
                            base->blockedByEnemy = true;
                        }
                    }
                }
                else if (building.buildCommandFailureFrames >= 240)
                {
                    Log::Get() << "Build of " << building << " failed due to own unit blocking or other error";
                }
                else
                {
                    it++;
                    continue;
                }

                // TODO: At some point we should restore the building placement, at least in some cases
                releaseBuilder(building);
                building.removeNoGoArea();
                it = pendingBuildings.erase(it);
                continue;
            }

            it++;
        }

        // Check for started buildings
        // TODO: Flip this around and only check the unit list when we have sent the build order
        if (!pendingBuildings.empty())
        {
            for (auto &unit : Units::allMine())
            {
                if (!unit->type.isBuilding() || unit->completed) continue;
                for (auto &pendingBuilding : pendingBuildings)
                {
                    if (pendingBuilding->unit) continue;
                    if (pendingBuilding->type != unit->type) continue;
                    if (pendingBuilding->tile != unit->getTilePosition()) continue;

                    pendingBuilding->constructionStarted(unit);
                    releaseBuilder(*pendingBuilding);
                    pendingBuilding->removeNoGoArea();
                }
            }
        }

        // Remove dead builders
        // The buildings themselves are pruned earlier
        for (auto it = builderQueues.begin(); it != builderQueues.end();)
        {
            if (!it->first->exists())
            {
                it = builderQueues.erase(it);
                continue;
            }

            it++;
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

    void build(BWAPI::UnitType type, BWAPI::TilePosition tile, const MyUnit &builder, int startFrame)
    {
        // Sanity check that we don't already have a pending building overlapping the tile
        // This happens if the producer orders two buildings on the same frame where one is using a build location converted from the other
        // When we detect this, we can just return; the producer will use a different build location on the next frame
        for (auto &pendingBuilding : pendingBuildings)
        {
            if (pendingBuilding->isConstructionStarted()) continue;
            if (Geo::Overlaps(tile, type.tileWidth(), type.tileHeight(),
                              pendingBuilding->tile, pendingBuilding->type.tileWidth(), pendingBuilding->type.tileHeight()))
            {
                return;
            }
        }

        auto building = std::make_shared<Building>(type, tile, builder, startFrame);
        pendingBuildings.push_back(building);
        builderQueues[builder].push_back(building);

        Workers::reserveWorker(builder);

        Log::Debug() << "Queued " << **pendingBuildings.rbegin() << " to start at " << startFrame << " for builder " << builder->id
                     << "; builder queue length: " << builderQueues[builder].size();

        BuildingPlacement::onBuildingQueued(building.get());
    }

    void cancel(BWAPI::TilePosition tile)
    {
        for (auto it = pendingBuildings.begin(); it != pendingBuildings.end(); it++)
        {
            if ((*it)->tile != tile) continue;

            // If construction has started, cancel it
            if ((*it)->unit)
            {
                Log::Get() << "Cancelling construction of " << *(*it)->unit;
                (*it)->unit->cancelConstruction();
            }

            (*it)->removeNoGoArea();

            BuildingPlacement::onBuildingCancelled(it->get());

            releaseBuilder(**it);
            pendingBuildings.erase(it);
            return;
        }
    }

    MyUnit getBuilderUnit(BWAPI::TilePosition tile, BWAPI::UnitType type, int *expectedArrivalFrame)
    {
        BWAPI::Position buildPosition = BWAPI::Position(tile) + BWAPI::Position(type.tileWidth() * 16, type.tileHeight() * 16);

        // First get the closest worker currently available for reassignment
        int bestMineralWorkerTravelTime = INT_MAX;
        MyUnit bestMineralWorker = Workers::getClosestReassignableWorker(buildPosition,
                                                                         !type.isResourceDepot(),
                                                                         &bestMineralWorkerTravelTime);

        // Next get the best existing builder
        MyUnit bestBuilder = nullptr;
        int bestBuilderTravelTime = INT_MAX;
        for (auto &builderAndQueue : builderQueues)
        {
            if (!builderAndQueue.first->exists()) continue;
            if (builderAndQueue.second.empty()) continue;

            int totalTravelTime = 0;

            // Sum up the travel time between the existing queued buildings
            BWAPI::Position lastPosition = builderAndQueue.first->lastPosition;
            for (const auto &building : builderAndQueue.second)
            {
                totalTravelTime +=
                        PathFinding::ExpectedTravelTime(lastPosition,
                                                        building->getPosition(),
                                                        builderAndQueue.first->type,
                                                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
                lastPosition = building->getPosition();
            }

            // Add in the travel time to this next building
            int expectedTravelTime = PathFinding::ExpectedTravelTime(lastPosition,
                                                                     buildPosition,
                                                                     builderAndQueue.first->type,
                                                                     PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                                     -1);
            if (expectedTravelTime == -1) continue; // Builder might be on an island
            totalTravelTime += expectedTravelTime;

            // Give a bonus to already-building workers, as we don't want to take a lot of workers off minerals
            if (totalTravelTime < bestBuilderTravelTime)
            {
                bestBuilderTravelTime = totalTravelTime;
                bestBuilder = builderAndQueue.first;
            }
        }

        // Finally get the best reserved builder
        MyUnit bestReserved = nullptr;
        int bestReservedTravelTime = INT_MAX;
        for (auto &reservedBuilder : reservedBuilders)
        {
            // Don't consider this again if it already has a queue
            if (builderQueues.contains(reservedBuilder)) continue;

            int expectedTravelTime = PathFinding::ExpectedTravelTime(reservedBuilder->lastPosition,
                                                                     buildPosition,
                                                                     reservedBuilder->type,
                                                                     PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                                     -1);
            if (expectedTravelTime == -1) continue; // Reserved builder might be on an island

            // Give a bonus to reserved builders (similar to above)
            if (expectedTravelTime < bestReservedTravelTime)
            {
                bestReservedTravelTime = expectedTravelTime;
                bestReserved = reservedBuilder;
            }
        }

        // Pick the best worker, giving existing builders a 6-second bonus
        if (bestReservedTravelTime < bestBuilderTravelTime)
        {
            bestBuilder = bestReserved;
            bestBuilderTravelTime = bestReservedTravelTime;
        }

        MyUnit builder;
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
            *expectedArrivalFrame = builder ? currentFrame + travelTime : -1;
        return builder;
    }

    std::vector<std::shared_ptr<Building>> &allPendingBuildings()
    {
        return pendingBuildings;
    }

    std::vector<Building *> pendingBuildingsOfType(BWAPI::UnitType type)
    {
        std::vector<Building *> result;
        for (const auto &building : pendingBuildings)
        {
            if (building->type == type) result.push_back(building.get());
        }

        return result;
    }

    void cancelBase(Base *base)
    {
        cancel(base->getTilePosition());

        // Also cancel assimilator if it is building
        for (auto &pendingBuilding : pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Assimilator))
        {
            if (base->hasGeyserOrRefineryAt(pendingBuilding->tile))
            {
                cancel(pendingBuilding->tile);
            }
        }
    }

    bool isPendingHere(BWAPI::TilePosition tile)
    {
        return std::any_of(pendingBuildings.begin(), pendingBuildings.end(), [&tile](const auto &pendingBuilding)
        {
            return pendingBuilding->tile == tile;
        });
    }

    Building *pendingHere(BWAPI::TilePosition tile)
    {
        for (const auto &building : pendingBuildings)
        {
            if (building->tile == tile) return building.get();
        }

        return nullptr;
    }

    bool hasPendingBuilding(const MyUnit &builder)
    {
        auto it = builderQueues.find(builder);
        if (it == builderQueues.end()) return false;

        return !it->second.empty();
    }

    void addReservedBuilder(const MyUnit &builder)
    {
        reservedBuilders.insert(builder);
    }

    void releaseReservedBuilder(const MyUnit &builder)
    {
        reservedBuilders.erase(builder);
    }

    bool isInEnemyStaticThreatRange(BWAPI::TilePosition tile, BWAPI::UnitType type)
    {
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());

        if (grid.staticGroundThreat(BWAPI::WalkPosition(tile)) > 0) return true;
        if (grid.staticGroundThreat(BWAPI::WalkPosition(tile + BWAPI::TilePosition(type.tileWidth(), 0))) > 0) return true;
        if (grid.staticGroundThreat(BWAPI::WalkPosition(tile + BWAPI::TilePosition(type.tileWidth(), type.tileHeight()))) > 0) return true;
        if (grid.staticGroundThreat(BWAPI::WalkPosition(tile + BWAPI::TilePosition(0, type.tileHeight()))) > 0) return true;

        return false;
    }

    int framesUntilCompleted(BWAPI::TilePosition tile, int defaultValue)
    {
        auto unit = Units::myBuildingAt(tile);
        if (unit)
        {
            if (unit->completed) return 0;
            return unit->estimatedCompletionFrame - currentFrame;
        }

        auto pendingBuilding = Builder::pendingHere(tile);
        if (pendingBuilding)
        {
            return pendingBuilding->expectedFramesUntilCompletion();
        }

        return defaultValue;
    }
}
