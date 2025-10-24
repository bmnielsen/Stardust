#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    void WorkerPathExploration::returning()
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
