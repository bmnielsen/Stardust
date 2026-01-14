#include "SingleWorkerModule.h"

#include <BWAPI/StateCopy.h>

#include "WorkerPathExploration.h"
#include "SimulateGatherPathTester.h"

namespace MiningOptimizationTraining
{
    namespace
    {
        BWAPI::StateCopy emptyStateCopy;
    }

    template<typename WorkerStatusType>
    bool SingleWorkerModule<WorkerStatusType>::initialize()
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
            BWAPI::Unit patch = nullptr;
            for (auto &mineralPatch : Map::getMyMain()->mineralPatches())
            {
                if (mineralPatch->tile == patchTile)
                {
                    patch = mineralPatch->getBwapiUnitIfVisible();
                    break;
                }
            }
            if (!patch) return false;

            // Initialize
            workerStatuses.emplace_back(std::make_unique<WorkerStatusType>(mapData, worker, patch, depot, nullptr, emptyStateCopy));
            worker->gather(patch);
        }

        return true;
    }

    template class SingleWorkerModule<WorkerPathExploration>;
    template class SingleWorkerModule<SimulateGatherPathTester>;
}
