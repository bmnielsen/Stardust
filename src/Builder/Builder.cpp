#include "Building.h"
#include "Workers.h"
#include "Units.h"
#include "PathFinding.h"
#include "Geo.h"
#include "BuildingPlacement.h"

namespace Builder
{
    namespace
    {
        std::vector<std::shared_ptr<Building>> pendingBuildings;
        std::map<MyUnit, std::vector<std::shared_ptr<Building>>> builderQueues;

        // Ensures the worker is being ordered to build this building
        void build(Building &building)
        {
            if (building.unit || !building.builder || !building.builder->exists()) return;

            // If the builder is constructing, we have already given it the build command and are waiting
            // for the building to appear
            if (building.builder->bwapiUnit->isConstructing()) return;

            // TODO: Detect cases where we have already issued a build command that succeeded
            // (e.g. spider mine blocking)

            // If we are close to the build position and want to build now, issue the build command
            int dist = Geo::EdgeToEdgeDistance(building.builder->type, building.builder->lastPosition, building.type, building.getPosition());
            if (dist <= 64 && building.desiredStartFrame - BWAPI::Broodwar->getFrameCount() <= BWAPI::Broodwar->getRemainingLatencyFrames())
            {
                // Return immediately if issuing the build command succeeded
                if (building.builder->build(building.type, building.tile)) return;

                // The build command failed
                // TODO: Look for blocking units
            }

            // Move towards the build position
#if DEBUG_UNIT_ORDERS
            CherryVis::log(building.builder->id) << "moveTo: Build location";
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

            // When the builder has no more buildings in its queue, release it back to Workers
            if (builderQueue.empty())
            {
                Workers::releaseWorker(builder);
                builderQueues.erase(builder);
                return;
            }

            // Tell the worker to build the new start of its queue
            build(**builderQueue.begin());
        }

        void writeInstrumentation()
        {
#if CHERRYVIS_ENABLED
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
                it = pendingBuildings.erase(it);
            }
            else
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

    void build(BWAPI::UnitType type, BWAPI::TilePosition tile, MyUnit builder, int startFrame)
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
        int bestTravelTime = INT_MAX;
        MyUnit bestWorker = Workers::getClosestReassignableWorker(buildPosition, true, &bestTravelTime);

        // Next see if any existing builder will be finished in time to reach the desired position faster
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
            totalTravelTime +=
                    PathFinding::ExpectedTravelTime(lastPosition,
                                                    buildPosition,
                                                    builderAndQueue.first->type,
                                                    PathFinding::PathFindingOptions::UseNearestBWEMArea);

            // Give a bonus to already-building workers, as we don't want to take a lot of workers off minerals
            if ((totalTravelTime / 2) < bestTravelTime)
            {
                bestTravelTime = totalTravelTime;
                bestWorker = builderAndQueue.first;
            }
        }

        if (expectedArrivalFrame)
            *expectedArrivalFrame = bestWorker ? BWAPI::Broodwar->getFrameCount() + bestTravelTime : -1;
        return bestWorker;
    }

    std::vector<Building *> allPendingBuildings()
    {
        std::vector<Building *> result;

        for (const auto &building : pendingBuildings)
        {
            result.push_back(building.get());
        }

        return result;
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

    bool isPendingHere(BWAPI::TilePosition tile)
    {
        for (const auto &building : pendingBuildings)
        {
            if (building->tile == tile) return true;
        }

        return false;
    }

    Building *pendingHere(BWAPI::TilePosition tile)
    {
        for (const auto &building : pendingBuildings)
        {
            if (building->tile == tile) return building.get();
        }

        return nullptr;
    }
}
