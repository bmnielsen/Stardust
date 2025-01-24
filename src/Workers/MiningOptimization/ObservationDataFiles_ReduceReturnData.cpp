#include "ObservationDataFiles.h"
#include "PathTraversalLoopGuard.h"

#define PRUNE_THRESHOLD 20

namespace WorkerMiningOptimization::ObservationDataFiles
{
    namespace
    {
        std::unordered_map<PositionAndVelocity, size_t> posToPreviousNodesCount;
        GameParameters gameParameters;

        template <typename T>
        unsigned long pruneThresholdForCountMap(const T &map)
        {
            unsigned long total = 0;
            for (const auto &[_, count] : map)
            {
                total += count;
            }
            if (total == 0) return 0;

            return 1 + ((total - 1) / PRUNE_THRESHOLD);
        }

        template <typename T>
        void pruneLowOccurrencesFromCountMap(T &map)
        {
            auto threshold = pruneThresholdForCountMap(map);
            for (auto it = map.begin(); it != map.end(); )
            {
                if (it->second < threshold)
                {
                    it = map.erase(it);
                }
                else
                {
                    it++;
                }
            }
        }

        // Recursively prunes a path
        // A path, or branch of a path, is pruned if it does not occur often compared to other paths
        // We also remove nodes that come before our exploration horizon
        void prune(const PositionAndVelocity &pos, // NOLINT(*-no-recursion)
                   std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &positionObservations,
                   PathTraversalLoopGuard &loopGuard,
                   bool pruningThisBranch = false,
                   bool pruningPreviousNode = true)
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
                        prune(nextPos, positionObservations, loopGuard, true, true);
                        loopGuard.pop(nextPos);
                    }

                    positionObservations.erase(positionDataIt);

                    return;
                }
            }

            // Determine if this node should be pruned for being before the exploration horizon
            bool shouldPruneForExplorationHorizon =
                    pruningPreviousNode && !hasMultiplePreviousNodes && (
                            positionDataIt->second.noResendArrivalObservations.empty() ||
                            positionDataIt->second.noResendArrivalObservations.largestArrivalDelay() >
                                (8 + gameParameters.latencyFrames + gameParameters.returnExploreBefore));

            auto pruneForExplorationHorizon = [&]()
            {
                if (!shouldPruneForExplorationHorizon) return;

                for (const auto &[nextPos, _] : nextPositionData)
                {
                    auto nextPosPreviousNodesIt = posToPreviousNodesCount.find(nextPos);
                    if (nextPosPreviousNodesIt != posToPreviousNodesCount.end() && nextPosPreviousNodesIt->second > 0)
                    {
                        nextPosPreviousNodesIt->second--;
                    }
                }

                positionObservations.erase(positionDataIt);

                return;
            };

            if (nextPositionData.empty())
            {
                pruneForExplorationHorizon();
                return;
            }

            if (nextPositionData.size() == 1)
            {
                prune(nextPositionData.begin()->first, positionObservations, loopGuard, false, shouldPruneForExplorationHorizon);
                loopGuard.pop(nextPositionData.begin()->first);
                pruneForExplorationHorizon();
                return;
            }

            auto threshold = pruneThresholdForCountMap(nextPositionData);
            if (threshold == 0) return;

            for (auto nextPosIt = nextPositionData.begin(); nextPosIt != nextPositionData.end(); )
            {
                if (nextPosIt->second < threshold)
                {
                    prune(nextPosIt->first, positionObservations, loopGuard, true, shouldPruneForExplorationHorizon);
                    loopGuard.pop(nextPosIt->first);
                    nextPosIt = nextPositionData.erase(nextPosIt);
                }
                else
                {
                    prune(nextPosIt->first, positionObservations, loopGuard, false, shouldPruneForExplorationHorizon);
                    loopGuard.pop(nextPosIt->first);
                    nextPosIt++;
                }
            }

            pruneForExplorationHorizon();
        }
    }

    void reduceReturnData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data)
    {
        gameParameters = getGameParameters();

        // Start by building a mapping of nodes to previous nodes with counts (they have their next nodes saved already)
        // Then starting with all nodes that have no previous nodes, prune branches that are encountered less than 5% of the time
        // Then scan and do the same for second resend positions, updating resend changes path as appropriate

        auto patches = data.size();
        int processed = 0;

        size_t totalPositions = 0;
        size_t totalPruned = 0;

        for (auto &[_, optimalReturnPositions] : data)
        {
            auto initialCount = optimalReturnPositions.size();
            totalPositions += initialCount;

            // Build the count of previous nodes for each node
            posToPreviousNodesCount.clear();
            for (const auto &[pos, returnPositionObservations] : optimalReturnPositions)
            {
                for (const auto &[nextPos, count] : returnPositionObservations.nextPositionAndOccurrences)
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

            for (const auto &[pos, metadata] : optimalReturnPositions)
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
                unsigned long threshold = std::ceil(averageConnectionsFromRootNodes / PRUNE_THRESHOLD);
                for (const auto &[pos, connections] : rootNodes)
                {
                    PathTraversalLoopGuard loopGuard;
                    prune(pos, optimalReturnPositions, loopGuard, connections < threshold);
                }
            }

            totalPruned += (initialCount - optimalReturnPositions.size());

            // Now prune all low-occurrence values from various counter maps
            for (auto &[_, metadata] : optimalReturnPositions)
            {
                pruneLowOccurrencesFromCountMap(metadata.resendArrivalObservations.arrivalDelayAndOccurrences);
                pruneLowOccurrencesFromCountMap(metadata.noResendArrivalObservations.arrivalDelayAndOccurrences);
            }

            processed++;
            if (processed % 5 == 0)
            {
                Log::Get() << "Processed " << processed << " of " << patches << " patches";
            }
        }

        Log::Get() << std::fixed << std::setprecision(2)
                   << "Pruned " << totalPruned << " of " << totalPositions << " total positions"
                   << " (" << ((double)totalPruned * 100.0/(double)totalPositions) << "%)";
    }
}
