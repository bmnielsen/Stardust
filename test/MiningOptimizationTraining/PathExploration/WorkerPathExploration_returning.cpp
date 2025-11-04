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

        // We check the patch collision status 15 frames after starting the return
        if (returnPositionHistory.size() == 15)
        {
            recordGatherCollisions();
        }
    }
}
