#pragma once

#include "PathExplorationModule.h"
#include "WorkerPathExploration.h"

namespace MiningOptimizationTraining
{
    // Module that just takes one of the initial workers and orders it to mine
    template <typename WorkerStatusType>
    class SingleWorkerModule : public PathExplorationModule<WorkerStatusType>
    {
    protected:
        using PathExplorationModule<WorkerStatusType>::workerStatuses;
        using PathExplorationModule<WorkerStatusType>::mapData;

        bool initialize() override
        {
            if (BWAPI::Broodwar->getFrameCount() == 0)
            {
                // Kill all but the leftmost initial worker
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isWorker() && unit->getPosition() != BWAPI::Position(240, 296))
                    {
                        BWAPI::Broodwar->killUnit(unit);
                    }
                }
            }

            // Give the workers time to die
            if (BWAPI::Broodwar->getFrameCount() < 10) return false;

            if (BWAPI::Broodwar->getFrameCount() == 10)
            {
                // Find the worker and depot
                BWAPI::Unit worker;
                BWAPI::Unit depot;
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isWorker()) worker = unit;
                    if (unit->getType().isResourceDepot()) depot = unit;
                }
                if (!worker) return false;
                if (!depot) return false;

                // Find the patch
                auto patch = (*Map::getMyMain()->mineralPatches().begin())->getBwapiUnitIfVisible();
                if (!patch) return false;

                // Initialize
                workerStatuses.emplace_back(std::make_unique<WorkerStatusType>(mapData, worker, patch, depot));
                worker->gather(patch);
            }

            return true;
        }
    };
}
