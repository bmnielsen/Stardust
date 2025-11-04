#include "SingleWorkerModule.h"

namespace MiningOptimizationTraining
{
    bool SingleWorkerModule::initialize()
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
            workerStatuses.emplace_back(mapData, worker, patch, depot);
            worker->gather(patch);
        }

        return true;
    }
}
