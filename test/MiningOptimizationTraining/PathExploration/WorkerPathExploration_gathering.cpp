#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    void WorkerPathExploration::gathering(MapData &mapData)
    {
        appendCurrentPosition(gatherPositionHistory);

        // If the worker has reached the WaitForMinerals frame, we can record the path information
        if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            recordGatherPath(mapData);
            return;
        }
    }

    void WorkerPathExploration::recordGatherPath(MapData &mapData)
    {
        auto parsedPositionHistory = parsePositionHistory(gatherPositionHistory, depot, patch);
        if (!parsedPositionHistory.valid) return;
    }
}
