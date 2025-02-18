#include "ObservationDataFiles.h"
#include "PathTraversalLoopGuard.h"

#define PRUNE_THRESHOLD 20

namespace WorkerMiningOptimization::ObservationDataFiles
{
    void reduceGatherData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data)
    {
    }
//
//    namespace
//    {
//        std::unordered_map<PositionAndVelocity, size_t> posToPreviousNodesCount;
//        GameParameters gameParameters;
//
//        template <typename T>
//        unsigned long pruneThresholdForCountMap(const T &map)
//        {
//            unsigned long total = 0;
//            for (const auto &[_, count] : map)
//            {
//                total += count;
//            }
//            if (total == 0) return 0;
//
//            return 1 + ((total - 1) / PRUNE_THRESHOLD);
//        }
//
//        template <typename T>
//        void pruneLowOccurrencesFromCountMap(T &map)
//        {
//            auto threshold = pruneThresholdForCountMap(map);
//            for (auto it = map.begin(); it != map.end(); )
//            {
//                if (it->second < threshold)
//                {
//                    it = map.erase(it);
//                }
//                else
//                {
//                    it++;
//                }
//            }
//        }
//
//        // Recursively prunes a path
//        // A path, or branch of a path, is pruned if it does not occur often compared to other paths
//        // We also remove nodes that come before our exploration horizon
//        void prune(const PositionAndVelocity &pos, // NOLINT(*-no-recursion)
//                   std::unordered_map<PositionAndVelocity, GatherPositionObservations> &positionObservations,
//                   PathTraversalLoopGuard &loopGuard,
//                   bool pruningThisBranch = false,
//                   bool pruningPreviousNode = true)
//        {
//            // If we detect a loop, return early
//            if (loopGuard.push(pos)) return;
//
//            // Look up whether this position has multiple previous nodes
//            // If so, and we are pruning this branch, we decrement the counter instead of deleting this node
//            auto previousNodesIt = posToPreviousNodesCount.find(pos);
//            bool hasMultiplePreviousNodes = (previousNodesIt != posToPreviousNodesCount.end() && previousNodesIt->second > 1);
//
//            // Look up the data for this position
//            auto positionDataIt = positionObservations.find(pos);
//            if (positionDataIt == positionObservations.end()) return; // Shouldn't happen
//
//            auto &nextPositionData = positionDataIt->second.nextPositionAndOccurrences;
//
//            // If pruning, recursively prune then delete this element
//            if (pruningThisBranch)
//            {
//                if (hasMultiplePreviousNodes)
//                {
//                    previousNodesIt->second--;
//                }
//                else
//                {
//                    for (const auto &[nextPos, _] : nextPositionData)
//                    {
//                        prune(nextPos, positionObservations, loopGuard, true, true);
//                        loopGuard.pop(nextPos);
//                    }
//
//                    positionObservations.erase(positionDataIt);
//
//                    return;
//                }
//            }
//
//            // Determine if this node should be pruned for being before the exploration horizon
//            bool shouldPruneForExplorationHorizon =
//                    pruningPreviousNode && !hasMultiplePreviousNodes && (
//                            positionDataIt->second.deltaToBenchmarkAndOccurrences.empty() ||
//                            positionDataIt->second.largestDeltaToBenchmark() < -gameParameters.gatherExploreBefore);
//
//            auto pruneForExplorationHorizon = [&]()
//            {
//                if (!shouldPruneForExplorationHorizon) return false;
//
//                for (const auto &[nextPos, _] : nextPositionData)
//                {
//                    auto nextPosPreviousNodesIt = posToPreviousNodesCount.find(nextPos);
//                    if (nextPosPreviousNodesIt != posToPreviousNodesCount.end() && nextPosPreviousNodesIt->second > 0)
//                    {
//                        nextPosPreviousNodesIt->second--;
//                    }
//                }
//
//                positionObservations.erase(positionDataIt);
//
//                return true;
//            };
//
//            if (nextPositionData.empty())
//            {
//                pruneForExplorationHorizon();
//                return;
//            }
//
//            if (nextPositionData.size() == 1)
//            {
//                prune(nextPositionData.begin()->first, positionObservations, loopGuard, false, shouldPruneForExplorationHorizon);
//                loopGuard.pop(nextPositionData.begin()->first);
//                pruneForExplorationHorizon();
//                return;
//            }
//
//            auto threshold = pruneThresholdForCountMap(nextPositionData);
//            if (threshold == 0) return;
//
//            bool prunedABranch = false;
//            for (auto nextPosIt = nextPositionData.begin(); nextPosIt != nextPositionData.end(); )
//            {
//                if (nextPosIt->second < threshold)
//                {
//                    prune(nextPosIt->first, positionObservations, loopGuard, true, shouldPruneForExplorationHorizon);
//                    loopGuard.pop(nextPosIt->first);
//                    nextPosIt = nextPositionData.erase(nextPosIt);
//                    prunedABranch = true;
//                }
//                else
//                {
//                    prune(nextPosIt->first, positionObservations, loopGuard, false, shouldPruneForExplorationHorizon);
//                    loopGuard.pop(nextPosIt->first);
//                    nextPosIt++;
//                }
//            }
//
//            if (pruneForExplorationHorizon()) return;
//
//            // If we pruned a branch, we need to also clean up the second resend positions
//            if (prunedABranch)
//            {
//                auto &secondResends = positionDataIt->second.secondResendObservations;
//
//                // Start by building the counts of previous nodes
//                std::unordered_map<PositionAndVelocity, size_t> secondResendPosToPreviousNodesCount;
//                for (const auto &[secondResendPos, secondResendData] : secondResends)
//                {
//                    for (const auto &[nextPos, count] : secondResendData.nextPositionAndOccurrences)
//                    {
//                        auto it = secondResendPosToPreviousNodesCount.find(nextPos);
//                        if (it == secondResendPosToPreviousNodesCount.end())
//                        {
//                            secondResendPosToPreviousNodesCount.emplace(nextPos, 1);
//                        }
//                        else
//                        {
//                            it->second++;
//                        }
//                    }
//                }
//
//                std::function<void(const PositionAndVelocity &)> removeSecondResendPath;
//                removeSecondResendPath = [&](const PositionAndVelocity &pos)
//                {
//                    // Decrement instead of removing if there are multiple previous nodes
//                    auto secondResendPreviousNodesIt = secondResendPosToPreviousNodesCount.find(pos);
//                    if (secondResendPreviousNodesIt != secondResendPosToPreviousNodesCount.end() && secondResendPreviousNodesIt->second > 1)
//                    {
//                        secondResendPreviousNodesIt->second--;
//                        return;
//                    }
//
//                    auto dataIt = secondResends.find(pos);
//                    if (dataIt == secondResends.end()) return; // Shouldn't happen
//
//                    // Recurse to the next positions
//                    for (const auto &[nextPos, _] : dataIt->second.nextPositionAndOccurrences)
//                    {
//                        removeSecondResendPath(nextPos);
//                    }
//
//                    // Remove this one
//                    secondResends.erase(dataIt);
//                };
//
//                // Now prune any starting nodes that no longer exist on the main path
//                // We restart the loop after each prune since it modifies the map
//                bool pruning = true;
//                while (pruning)
//                {
//                    pruning = false;
//                    for (const auto &[secondResendPos, secondResendPosData] : secondResends)
//                    {
//                        if (secondResendPosData.deltaToFirstResend == 1 && !nextPositionData.contains(secondResendPos))
//                        {
//                            pruning = true;
//                            removeSecondResendPath(secondResendPos);
//                            break;
//                        }
//                    }
//                }
//            }
//        }
//
//        bool checkResendChangesPath(GatherPositionObservations &metadata,
//                                    std::unordered_map<PositionAndVelocity, GatherPositionObservations> &allMetadata)
//        {
//            // First map all of the second resend positions by their delta to the first resend
//            // Return if we detect any instability in the path
//            std::unordered_map<size_t, PositionAndVelocity> deltaToPos;
//            size_t maxDelta = 0;
//            for (const auto &[secondResendPos, secondResendData] : metadata.secondResendObservations)
//            {
//                if (secondResendData.nextPositionAndOccurrences.size() > 1) return false;
//
//                auto deltaHere = (size_t)secondResendData.deltaToFirstResend;
//                if (deltaToPos.contains(deltaHere)) return false;
//
//                maxDelta = std::max(maxDelta, deltaHere);
//                deltaToPos[deltaHere] = secondResendPos;
//            }
//            if (deltaToPos.size() != maxDelta) return false;
//
//            // Now compare the path to the normal non-resend path
//            auto noResendPath = metadata.followingPositionsIfStable(allMetadata);
//            if (noResendPath.empty()) return false;
//
//            bool pathsMatch = true;
//            for (size_t noResendPathIdx = 0; noResendPathIdx < std::min(maxDelta, noResendPath.size() - 1); noResendPathIdx++)
//            {
//                if (noResendPath[noResendPathIdx]->pos != deltaToPos[noResendPathIdx + 1])
//                {
//                    pathsMatch = false;
//                    break;
//                }
//            }
//            if (!pathsMatch) return false;
//
//            metadata.resendChangesPath = ResendChangesPath::No;
//            metadata.secondResendObservations.clear();
//            return true;
//        }
//    }
//
//    void reduceGatherData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data)
//    {
//        gameParameters = getGameParameters();
//
//        // Start by building a mapping of nodes to previous nodes with counts (they have their next nodes saved already)
//        // Then starting with all nodes that have no previous nodes, prune branches that are encountered less than 5% of the time
//        // Then scan and do the same for second resend positions, updating resend changes path as appropriate
//
//        auto patches = data.size();
//        int processed = 0;
//
//        size_t totalPositions = 0;
//        size_t totalPruned = 0;
//        size_t totalResendChangesPath = 0;
//        size_t totalResendChangesPathReset = 0;
//
//        for (auto &[_, optimalGatherPositions] : data)
//        {
//            auto initialCount = optimalGatherPositions.size();
//            totalPositions += initialCount;
//
//            // Build the count of previous nodes for each node
//            posToPreviousNodesCount.clear();
//            for (const auto &[pos, gatherPositionObservations] : optimalGatherPositions)
//            {
//                for (const auto &[nextPos, count] : gatherPositionObservations.nextPositionAndOccurrences)
//                {
//                    auto it = posToPreviousNodesCount.find(nextPos);
//                    if (it == posToPreviousNodesCount.end())
//                    {
//                        posToPreviousNodesCount.emplace(nextPos, 1);
//                    }
//                    else
//                    {
//                        it->second++;
//                    }
//                }
//            }
//
//            // Prune recursively from all root nodes
//            std::vector<std::pair<PositionAndVelocity, uint16_t>> rootNodes;
//            unsigned long totalConnectionsFromRootNodes = 0;
//
//            for (const auto &[pos, metadata] : optimalGatherPositions)
//            {
//                if (!posToPreviousNodesCount.contains(pos))
//                {
//                    uint16_t total = 0;
//                    for (const auto &[_, count] : metadata.nextPositionAndOccurrences)
//                    {
//                        total += count;
//                    }
//
//                    if (total > 0)
//                    {
//                        rootNodes.emplace_back(pos, total);
//                        totalConnectionsFromRootNodes += total;
//                    }
//                }
//            }
//
//            if (!rootNodes.empty())
//            {
//                double averageConnectionsFromRootNodes = ((double)totalConnectionsFromRootNodes / (double)rootNodes.size());
//                unsigned long threshold = std::ceil(averageConnectionsFromRootNodes / PRUNE_THRESHOLD);
//                for (const auto &[pos, connections] : rootNodes)
//                {
//                    PathTraversalLoopGuard loopGuard;
//                    prune(pos, optimalGatherPositions, loopGuard, connections < threshold);
//                }
//            }
//
//            totalPruned += (initialCount - optimalGatherPositions.size());
//
//            // Now look at second resend data
//            for (auto &[_, metadata] : optimalGatherPositions)
//            {
//                if (metadata.resendChangesPath != ResendChangesPath::Yes) continue;
//                totalResendChangesPath++;
//
//                if (checkResendChangesPath(metadata, optimalGatherPositions))
//                {
//                    totalResendChangesPathReset++;
//                }
//            }
//
//            // Finally prune all low-occurrence values from various counter maps
//            for (auto &[_, metadata] : optimalGatherPositions)
//            {
//                pruneLowOccurrencesFromCountMap(metadata.deltaToBenchmarkAndOccurrences);
//                pruneLowOccurrencesFromCountMap(metadata.noSecondResendArrivalObservations.arrivalDelayAndOccurrences);
//                for (auto &[_, secondPosMetadata] : metadata.secondResendObservations)
//                {
//                    pruneLowOccurrencesFromCountMap(secondPosMetadata.arrivalObservations.arrivalDelayAndOccurrences);
//                }
//            }
//
//            processed++;
//            if (processed % 5 == 0)
//            {
//                Log::Get() << "Processed " << processed << " of " << patches << " patches";
//            }
//        }
//
//        Log::Get() << std::fixed << std::setprecision(2)
//                   << "Pruned " << totalPruned << " of " << totalPositions << " total positions"
//                   << " (" << ((double)totalPruned * 100.0/(double)totalPositions) << "%)";
//        if (totalResendChangesPath > 0)
//        {
//            Log::Get() << std::fixed << std::setprecision(2)
//                       << "Reset resend changes path on " << totalResendChangesPathReset << " of " << totalResendChangesPath << " positions"
//                       << " (" << ((double)totalResendChangesPathReset * 100.0/(double)totalResendChangesPath) << "%)";
//        }
//    }
}
