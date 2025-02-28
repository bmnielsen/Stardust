#include "ObservationDataFiles.h"
#include "PathTraversalLoopGuard.h"

// The threshold at which to prune branches, as a fraction of 255
// So for example, if the threshold is set to 20, we will prune any branches that appear less than 10/255 of the time
#define PRUNE_THRESHOLD 2

namespace WorkerMiningOptimization::ObservationDataFiles
{
    namespace
    {
        void pruneNextNodes(std::vector<ReturnPositionObservations> &nextNodes) // NOLINT(*-no-recursion)
        {
            for (auto nextNodeIt = nextNodes.begin(); nextNodeIt != nextNodes.end(); )
            {
                if (nextNodeIt->occurrenceRate < PRUNE_THRESHOLD)
                {
                    nextNodeIt = nextNodes.erase(nextNodeIt);
                }
                else
                {
                    nextNodeIt++;
                }
            }

            updateNextOccurenceRates(nextNodes);

            for (auto &nextNode : nextNodes)
            {
                pruneNextNodes(nextNode.nextPositions);
            }
        }
    }

    void reduceReturnData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data)
    {
        // Starting at each root node, recursively prune any branches that have an occurrence rate below the threshold
        for (auto &[_, rootNodes] : data)
        {
            for (auto &[_, rootNode] : rootNodes)
            {
                pruneNextNodes(rootNode.nextPositions);
            }
        }
    }
}
