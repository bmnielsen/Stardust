#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    void WorkerPathExploration::gathering()
    {
        appendCurrentPosition(gatherPositionHistory);

        // If the worker has reached the WaitForMinerals frame, we can record the path information
        if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            recordGatherPath();
            return;
        }
    }

    void WorkerPathExploration::recordGatherPath()
    {
        auto parsedPositionHistory = parsePositionHistory(gatherPositionHistory, depot, patch);
        if (!parsedPositionHistory.valid) return;

        // Find or create the root node
        auto &rootNodes = mapData.resourceToGatherRootNodes[TilePosition::fromBWAPI(patch->getTilePosition())];
        auto rootNodeIt = rootNodes.find(**gatherPositionHistory.begin());
        if (rootNodeIt == rootNodes.end())
        {
            auto result = rootNodes.emplace(**gatherPositionHistory.begin(), GatherObservations{**gatherPositionHistory.begin()});
            rootNodeIt = result.first;
        }
        auto &rootNode = rootNodeIt->second;
        if (rootNode.occurrences < UINT32_MAX) rootNode.occurrences++;

    }
}
