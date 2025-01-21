#include "WorkerMiningOptimization.h"

#include "PathTraversalLoopGuard.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        std::unordered_map<PositionAndVelocity, size_t> posToPreviousNodesCount;

        // Recursively prunes a path
        void prune(const PositionAndVelocity &pos, // NOLINT(*-no-recursion)
                   std::unordered_map<PositionAndVelocity, GatherPositionObservations> &positionObservations,
                   PathTraversalLoopGuard &loopGuard,
                   bool pruningThisBranch = false)
        {
            // If we detect a loop, return early
            if (loopGuard.push(pos)) return;

            // Look up whether this position has multiple previous nodes
            // If so, and we are pruning this branch, we decrement the counter instead of deleting this node
            auto previousNodesIt = posToPreviousNodesCount.find(pos);
            bool hasMultiplePreviousNodes = (previousNodesIt != posToPreviousNodesCount.end() && previousNodesIt->second > 1);

            // Look up the data for this position
            auto positionDataIt = positionObservations.find(pos);
            if (positionDataIt == positionObservations.end()) return; // Shouldn't happen

            auto &nextPositionData = positionDataIt->second.nextPositionAndOccurrences;

            // If pruning, recursively prune then delete this element
            if (pruningThisBranch)
            {
                if (hasMultiplePreviousNodes)
                {
                    previousNodesIt->second--;
                }
                else
                {
                    for (const auto &[nextPos, _] : nextPositionData)
                    {
                        prune(nextPos, positionObservations, loopGuard, true);
                        loopGuard.pop(nextPos);
                    }

                    positionObservations.erase(positionDataIt);

                    return;
                }
            }

            if (nextPositionData.empty()) return;
            if (nextPositionData.size() == 1)
            {
                prune(nextPositionData.begin()->first, positionObservations, loopGuard);
                loopGuard.pop(nextPositionData.begin()->first);
                return;
            }

            uint16_t total = 0;
            for (const auto &[_, count] : nextPositionData)
            {
                total += count;
            }
            if (total == 0) return;

            bool prunedABranch = false;
            uint16_t threshold = 1 + ((total - 1) / 20);
            for (auto nextPosIt = nextPositionData.begin(); nextPosIt != nextPositionData.end(); )
            {
                if (nextPosIt->second < threshold)
                {
                    prune(nextPosIt->first, positionObservations, loopGuard, true);
                    loopGuard.pop(nextPosIt->first);
                    nextPosIt = nextPositionData.erase(nextPosIt);
                    prunedABranch = true;
                }
                else
                {
                    prune(nextPosIt->first, positionObservations, loopGuard);
                    loopGuard.pop(nextPosIt->first);
                    nextPosIt++;
                }
            }

            // If we pruned a branch, we need to also clean up the second resend positions
            if (prunedABranch)
            {
                auto &secondResends = positionDataIt->second.secondResendObservations;

                // Start by building the counts of previous nodes
                std::unordered_map<PositionAndVelocity, size_t> secondResendPosToPreviousNodesCount;
                for (const auto &[secondResendPos, secondResendData] : secondResends)
                {
                    for (const auto &[nextPos, count] : secondResendData.nextPositionAndOccurrences)
                    {
                        auto it = secondResendPosToPreviousNodesCount.find(nextPos);
                        if (it == secondResendPosToPreviousNodesCount.end())
                        {
                            secondResendPosToPreviousNodesCount.emplace(nextPos, 1);
                        }
                        else
                        {
                            it->second++;
                        }
                    }
                }

                std::function<void(const PositionAndVelocity &)> removeSecondResendPath;
                removeSecondResendPath = [&](const PositionAndVelocity &pos)
                {
                    // Decrement instead of removing if there are multiple previous nodes
                    auto secondResendPreviousNodesIt = secondResendPosToPreviousNodesCount.find(pos);
                    if (secondResendPreviousNodesIt != secondResendPosToPreviousNodesCount.end() && secondResendPreviousNodesIt->second > 1)
                    {
                        secondResendPreviousNodesIt->second--;
                        return;
                    }

                    auto dataIt = secondResends.find(pos);
                    if (dataIt == secondResends.end()) return; // Shouldn't happen

                    // Recurse to the next positions
                    for (const auto &[nextPos, _] : dataIt->second.nextPositionAndOccurrences)
                    {
                        removeSecondResendPath(nextPos);
                    }

                    // Remove this one
                    secondResends.erase(dataIt);
                };

                // Now prune any starting nodes that no longer exist on the main path
                // We restart the loop after each prune since it modifies the map
                bool pruning = true;
                while (pruning)
                {
                    pruning = false;
                    for (const auto &[secondResendPos, secondResendPosData] : secondResends)
                    {
                        if (secondResendPosData.deltaToFirstResend == 1 && !nextPositionData.contains(secondResendPos))
                        {
                            pruning = true;
                            removeSecondResendPath(secondResendPos);
                            break;
                        }
                    }
                }
            }
        }
    }

    void reduceGatherData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data)
    {
        // Start by building a mapping of nodes to previous nodes with counts (they have their next nodes saved already)
        // Then starting with all nodes that have no previous nodes, prune branches that are encountered less than 5% of the time
        // Then scan and do the same for second resend positions, updating resend changes path as appropriate

        auto patches = data.size();
        int processed = 0;

        size_t totalPositions = 0;
        size_t totalPruned = 0;

        for (auto &[_, optimalGatherPositions] : data)
        {
            auto initialCount = optimalGatherPositions.size();
            totalPositions += initialCount;

            // Build the count of previous nodes for each node
            posToPreviousNodesCount.clear();
            for (const auto &[pos, gatherPositionObservations] : optimalGatherPositions)
            {
                for (const auto &[nextPos, count] : gatherPositionObservations.nextPositionAndOccurrences)
                {
                    auto it = posToPreviousNodesCount.find(nextPos);
                    if (it == posToPreviousNodesCount.end())
                    {
                        posToPreviousNodesCount.emplace(nextPos, 1);
                    }
                    else
                    {
                        it->second++;
                    }
                }
            }

            // Prune recursively from all root nodes
            std::vector<std::pair<PositionAndVelocity, uint16_t>> rootNodes;
            unsigned long totalConnectionsFromRootNodes = 0;

            for (const auto &[pos, metadata] : optimalGatherPositions)
            {
                if (!posToPreviousNodesCount.contains(pos))
                {
                    uint16_t total = 0;
                    for (const auto &[_, count] : metadata.nextPositionAndOccurrences)
                    {
                        total += count;
                    }

                    if (total > 0)
                    {
                        rootNodes.emplace_back(pos, total);
                        totalConnectionsFromRootNodes += total;
                    }
                }
            }

            if (!rootNodes.empty())
            {
                double averageConnectionsFromRootNodes = ((double)totalConnectionsFromRootNodes / (double)rootNodes.size());
                unsigned long threshold = std::ceil(0.05 * averageConnectionsFromRootNodes);
                for (const auto &[pos, connections] : rootNodes)
                {
                    PathTraversalLoopGuard loopGuard;
                    prune(pos, optimalGatherPositions, loopGuard, connections < threshold);
                }
            }

            totalPruned += (initialCount - optimalGatherPositions.size());

            // Now look at second resend data
            // First check if the

            processed++;
            if (processed % 5 == 0)
            {
                Log::Get() << "Processed " << processed << " of " << patches << " patches";
            }
        }

        Log::Get() << "Pruned " << totalPruned << " of " << totalPositions << " total positions"
                   << std::fixed << std::setprecision(2) << " (" << ((double)totalPruned * 100.0/(double)totalPositions) << "%)";
    }
}
