#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    void WorkerPathExploration::returning(MapData &mapData)
    {
        // If the worker has delivered its minerals, we can record the path information
        if (!worker->isCarryingMinerals())
        {
            // TODO: Record path
            return;
        }

        appendCurrentPosition(returnPositionHistory);
    }
}
