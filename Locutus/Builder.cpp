#include "Builder.h"

#include "Building.h"
#include "Workers.h"
#include "Units.h"
#include "PathFinding.h"

namespace Builder
{
    namespace
    {
        std::vector<Building> pendingBuildings;
        std::map<BWAPI::Unit, std::vector<Building*>> builderQueues;

        // Ensures the worker is being ordered to build this building
        void build(Building * building)
        {
            if (building->unit || !building->builder || !building->builder->exists()) return;

            // If the builder is constructing, we have already given it the build command and are waiting
            // for the building to appear
            if (building->builder->isConstructing()) return;

            // Move if we aren't close to the build position
            int dist = building->builder->getDistance(building->getPosition());
            if (dist < 64)
            {
                Units::getMine(building->builder).moveTo(building->getPosition());
                return;
            }

            // TODO: Detect cases where we have already issued a build command that succeeded
            // (e.g. spider mine blocking)

            // Issue the build command
            bool result = building->builder->build(building->type, building->tile);

            // Build command failed
            if (!result)
            {
                // TODO: Look for blocking units

                Units::getMine(building->builder).moveTo(building->getPosition());
            }
        }

        // Releases the builder from constructing the given building
        // The builder may still have more buildings in its queue
        void releaseBuilder(Building * building)
        {
            auto builder = building->builder;
            building->builder = nullptr;

            // Remove the building from the builder's queue
            auto & builderQueue = builderQueues[builder];
            for (auto it = builderQueue.begin(); it != builderQueue.end(); )
            {
                it = *it == building ? builderQueue.erase(it) : it + 1;
            }

            // When the builder has no more buildings in its queue, release it back to Workers
            if (builderQueue.empty())
            {
                Workers::releaseWorker(builder);
                builderQueues.erase(builder);
                return;
            }

            // Tell the worker to build the new start of its queue
            build(*builderQueue.begin());
        }
    }

    void update()
    {
        // Prune the list of pending buildings
        for (auto it = pendingBuildings.begin(); it != pendingBuildings.end(); )
        {
            // Remove the building if:
            // - The building has completed
            // - The building has died while under construction
            // - The builder has died before starting construction
            // TODO: Cancel dying buildings
            if ((it->unit && (!it->unit->exists() || it->unit->isCompleted()))
                || (!it->unit && (!it->builder->exists() || it->builder->getPlayer() != BWAPI::Broodwar->self())))
            {
                it = pendingBuildings.erase(it);
            }
            else
                it++;
        }

        // Check for started buildings
        // TODO: Flip this around and only check the unit list when we have sent the build order
        if (!pendingBuildings.empty())
        {
            for (auto & unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->getType().isBuilding() || unit->isCompleted()) continue;
                for (auto & pendingBuilding : pendingBuildings)
                {
                    if (pendingBuilding.unit) continue;
                    if (pendingBuilding.type != unit->getType()) continue;
                    if (pendingBuilding.tile != unit->getTilePosition()) continue;

                    pendingBuilding.unit = unit;
                    releaseBuilder(&pendingBuilding);
                }
            }
        }

        // Update the builders
        for (auto it = builderQueues.begin(); it != builderQueues.end(); )
        {
            // Dead builders
            // The building itself is pruned earlier
            if (!it->first->exists() || it->first->getPlayer() != BWAPI::Broodwar->self())
            {
                it = builderQueues.erase(it);
                continue;
            }

            build(*it->second.begin());
        }
    }

    BWAPI::Unit getBuilderUnit(BWAPI::TilePosition tile, BWAPI::UnitType type, int * expectedArrivalFrame)
    {
        BWAPI::Position buildPosition = BWAPI::Position(tile) + BWAPI::Position(type.tileWidth() * 16, type.tileHeight() * 16);

        // First get the closest worker currently available for reassignment
        int bestTravelTime = INT_MAX;
        BWAPI::Unit bestWorker = nullptr;
        for (auto & unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (!Workers::isAvailableBuilder(unit)) continue;

            int travelTime = 
                PathFinding::ExpectedTravelTime(unit->getPosition(), buildPosition, unit->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (travelTime < bestTravelTime)
            {
                bestTravelTime = travelTime;
                bestWorker = unit;
            }
        }

        // Next see if any existing builder will be finished in time to reach the desired position faster
        for (auto & builderAndQueue : builderQueues)
        {
            if (!builderAndQueue.first->exists()) continue;
            if (builderAndQueue.second.empty()) continue;

            int totalTravelTime = 0;

            // Sum up the travel time between the existing queued buildings
            BWAPI::Position lastPosition = builderAndQueue.first->getPosition();
            for (auto building : builderAndQueue.second)
            {
                totalTravelTime +=
                    PathFinding::ExpectedTravelTime(lastPosition, building->getPosition(), builderAndQueue.first->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
                lastPosition = building->getPosition();
            }

            // Add in the travel time to this next building
            totalTravelTime += 
                PathFinding::ExpectedTravelTime(lastPosition, buildPosition, builderAndQueue.first->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);

            if (totalTravelTime < bestTravelTime)
            {
                bestTravelTime = totalTravelTime;
                bestWorker = builderAndQueue.first;
            }
        }

        if (expectedArrivalFrame)
            *expectedArrivalFrame = bestWorker ? BWAPI::Broodwar->getFrameCount() + bestTravelTime : -1;
        return bestWorker;
    }
}
