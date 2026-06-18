#include "ExploreStartPositionsModule.h"

#include <BWAPI/SimulateGatherPathOptions.h>
#include <BWAPI/SimulateGatherPathResult.h>

#include "MiningOptimizationTraining/DataModel/Configuration.h"
#include "PathExplorationUtils.h"

namespace MiningOptimizationTraining
{
    namespace
    {
        // Container for keeping track of the relevant limits for the phase of exploration we are in (gather vs. return)
        struct Limits
        {
            int startOfExplorationWindow;
            int endOfExplorationWindow;
            int resends;
        };

        // Gets the total occurrences from a next positions vector or arrival observations map
        uint32_t getTotalOccurrences(const auto &observations)
        {
            uint32_t totalOccurrences = 0;
            for (const auto &[_, occurrences] : observations) totalOccurrences += occurrences;
            return totalOccurrences;
        }

        // Gets the next path node matching a specific next position
        // If update is set, nodes are created where they don't exist and occurrence counts are incremented
        template <typename ObservationType>
        PathNode<ObservationType> *getNextPathNode(std::vector<std::pair<PathNode<ObservationType>, uint32_t>> &nextPositions,
                                                   BWAPI::ExactPosition position,
                                                   bool update)
        {
            PositionAndVelocity pos(position);

            std::pair<PathNode<ObservationType>, uint32_t> *nextPathNodePair = nullptr;
            for (auto &pathNodePair : nextPositions)
            {
                if (pathNodePair.first.pos == pos)
                {
                    nextPathNodePair = &pathNodePair;
                    break;
                }
            }

            if (!update && !nextPathNodePair) return nullptr;

            if (!nextPathNodePair)
            {
                nextPathNodePair = &nextPositions.emplace_back(PathNode<ObservationType>{pos}, 0);
            }

            if (update && getTotalOccurrences(nextPositions) < UINT32_MAX)
            {
                nextPathNodePair->second++;
            }

            return &(nextPathNodePair->first);
        }

        // Adds an arrival observation to the given observations map
        template <typename ObservationType>
        const ObservationType& addArrivalObservation(std::map<ObservationType, uint32_t> &observations,
                                                     const ObservationType &arrivalData,
                                                     uint32_t *occurrences = nullptr)
        {
            auto dataIt = observations.find(arrivalData);
            if (dataIt == observations.end())
            {
                dataIt = observations.emplace(arrivalData, 0).first;
            }

            if (getTotalOccurrences(observations) < UINT32_MAX) dataIt->second++;

            if (occurrences) *occurrences = dataIt->second;

            return dataIt->first;
        }

        // Gets the number of times a root node has been explored
        template <typename ObservationType>
        uint32_t getTimesExplored(const std::unordered_map<PositionAndVelocity, Path<ObservationType>> &pathRootNodes,
                                  const PositionAndVelocity &pos,
                                  bool collision)
        {
            auto it = pathRootNodes.find(pos);
            if (it == pathRootNodes.end()) return 0;
            return (collision ? it->second.timesExploredWithCollision : it->second.timesExploredWithNoCollision);
        }

        // Checks if two paths are equal, can be called with a vector or range
        bool pathsEqual(auto first, auto second)
        {
            std::input_or_output_iterator auto firstIt = first.begin();
            std::input_or_output_iterator auto secondIt = second.begin();
            while (firstIt != first.end() && secondIt != second.end())
            {
                if (*firstIt != *secondIt) return false;
                firstIt++;
                secondIt++;
            }
            return firstIt == first.end() && secondIt == second.end();
        }

        // Records the results of exploring a node
        template <typename ObservationType>
        struct NodeExplorationResult
        {
            ObservationType arrivalData;
            BWAPI::ExactPosition nextPathStartPositionActionAtArrival;
            int actionFrameAtArrival;
            int lastOrderProcessTimerOverrideFrameAtArrival;
            BWAPI::ExactPosition nextPathStartPositionActionAfterArrival;
            int actionFrameAfterArrival;
            int lastOrderProcessTimerOverrideFrameAfterArrival;
            std::set<int> resends;
        };
    }

    template <>
    void ExploreStartPositionsModule<ExploreStartPosition>::initializeStartPositions()
    {
        // Read the remaining start positions from all of the patches in scope
        for (auto base : Map::allBases())
        {
            if (options.oneBase && base->getTilePosition() != options.oneBase) continue;

            for (auto &patch : base->mineralPatches())
            {
                if (options.onePatch && patch->tile != options.onePatch) continue;

                auto patchUnit = patch->getBwapiUnitIfVisible();
                if (!patchUnit)
                {
                    Log::Get() << "ERROR: Could not find unit for patch @ " << patch->tile;
                    return;
                }

                for (auto &[_, path] :
                        mapData.resourceToReturnPaths[TilePosition(patch->tile.x, patch->tile.y)])
                {
                    for (auto it = path.positionsToExplore.rbegin(); it != path.positionsToExplore.rend(); it++)
                    {
                        startPositions.emplace_back(ExploreStartPosition{*it, patchUnit});
                    }
                }
            }
        }
    }

    template <>
    void ExploreStartPositionsModule<ExploreStartPosition>::explore(ExploreStartPosition &startPosition)
    {
        auto &patch = startPosition.patch;

        auto &gatherData = mapData.resourceToGatherPaths[TilePosition::fromBWAPI(patch->getTilePosition())];
        auto &returnData = mapData.resourceToReturnPaths[TilePosition::fromBWAPI(patch->getTilePosition())];

        // Remove this position from the pending list
        auto &path = returnData[PositionAndVelocity(startPosition.pos)];
        if (path.positionsToExplore.empty() || (*path.positionsToExplore.rbegin()) != startPosition.pos)
        {
            Log::Get() << "ERROR: Unexpectedly didn't find position on path's positions to explore";
            return;
        }
        path.positionsToExplore.pop_back();

        auto createGatherArrivalData = [&](
                const auto &simulatedPathWithActionAtArrival,
                const auto &simulatedPathWithActionAfterArrival,
                const BWAPI::ExactPosition &currentPosition)
        {
            return GatherArrivalData::createFromSimulatedPaths(
                simulatedPathWithActionAtArrival,
                simulatedPathWithActionAfterArrival,
                patch,
                currentPosition);
        };

        auto createReturnArrivalData = [&](
                const auto &simulatedPathWithActionAtArrival,
                const auto &simulatedPathWithActionAfterArrival,
                const BWAPI::ExactPosition &)
        {
            return ReturnArrivalData::createFromSimulatedPaths(simulatedPathWithActionAtArrival, simulatedPathWithActionAfterArrival);
        };

        auto makePathObservations = [&]<typename ObservationType>(
                const Limits &limits,
                const MiningOptimization::CannonPlacement &cannonPlacement,
                auto &createArrivalData,
                std::unordered_map<PositionAndVelocity, Path<ObservationType>> &rootNodes,
                int startFrame,
                std::unique_ptr<bwgame::state> &initialState,
                BWAPI::ExactPosition startPosition,
                std::vector<NodeExplorationResult<ObservationType>> &results)
        {
            // Get or create the root node
            auto currentPositionAndVelocity = PositionAndVelocity(startPosition);
            auto rootNodeIt = rootNodes.find(currentPositionAndVelocity);
            if (rootNodeIt == rootNodes.end())
            {
                rootNodeIt = rootNodes.emplace(currentPositionAndVelocity, Path<ObservationType>{currentPositionAndVelocity}).first;
            }
            auto &rootNode = rootNodeIt->second;

            // Explores the path, recursively going down a level as appropriate
            auto explorePath = [&]( // NOLINT(*-no-recursion)
                    auto &explorePath,
                    int frame,
                    BWAPI::ExactPosition currentPosition,
                    std::vector<std::pair<PathNode<ObservationType>, uint32_t>> *nextPositions,
                    std::set<int> resendFrames = {},
                    PathNode<ObservationType> *resendNode = nullptr,
                    std::ranges::subrange<std::vector<BWAPI::ExactPosition>::iterator> noResendPath = {})
            {
                // Simulate the path
                auto simulate = [&](bool forceAction)
                {
                    return simWorker->simulateGatherPath(
                            BWAPI::SimulateGatherPathOptions(resendFrames, initialState).setForceAction(forceAction));
                };

                // We both simulate with action at arrival and action after arrival
                auto simulatedPathWithDeliveryAtArrivalResult = simulate(true);
                auto simulatedPathWithDeliveryAfterArrivalResult = simulate(false);
                if (!simulatedPathWithDeliveryAtArrivalResult || !simulatedPathWithDeliveryAfterArrivalResult)
                {
                    Log::Get() << "ERROR: Path could not be simulated";
                    return;
                }
                auto &simulatedPath = simulatedPathWithDeliveryAtArrivalResult->positions;
                ObservationType arrivalData =
                        createArrivalData(*simulatedPathWithDeliveryAtArrivalResult,
                                          *simulatedPathWithDeliveryAfterArrivalResult,
                                          currentPosition);

                // If this is the no-resend path, record the appropriate exploration on the root node
                if (resendFrames.empty())
                {
                    bool collision = PathExplorationUtils::detectCollision(simulatedPath);
                    if (collision)
                    {
                        rootNode.timesExploredWithCollision++;
                    }
                    else
                    {
                        rootNode.timesExploredWithNoCollision++;
                    }
                }

                auto addResult = [&]()
                {
                    // Reset the arrival delay since it might have been updated while exploring
                    arrivalData.setArrivalDelay(simulatedPath.size());

                    // This is always called last, so we can use move semantics
                    results.emplace_back(std::move(arrivalData),
                                         std::move(simulatedPathWithDeliveryAtArrivalResult->nextPathStartPosition),
                                         simulatedPathWithDeliveryAtArrivalResult->actionFrame,
                                         simulatedPathWithDeliveryAtArrivalResult->lastOrderProcessTimerOverrideFrame,
                                         std::move(simulatedPathWithDeliveryAfterArrivalResult->nextPathStartPosition),
                                         simulatedPathWithDeliveryAfterArrivalResult->actionFrame,
                                         simulatedPathWithDeliveryAfterArrivalResult->lastOrderProcessTimerOverrideFrame,
                                         std::move(resendFrames));
                };

                // If this is a resend node, we have a couple of additional steps to do
                if (resendNode)
                {
                    // If this node is uninitialized, check if the resend changed the path or not
                    if (resendNode->type == NodeType::Uninitialized || resendNode->type == NodeType::StableNode)
                    {
                        if (pathsEqual(noResendPath, simulatedPath))
                        {
                            resendNode->type = NodeType::StableNode;

                            // We can just jump out now, since stable nodes don't need to be explored for resends
                            return;
                        }

                        // Set the type of resend node
                        if (noResendPath.size() > limits.startOfExplorationWindow && noResendPath.size() < simulatedPath.size())
                        {
                            resendNode->type = NodeType::PoorResendNode;
                        }
                        else if (resendFrames.size() >= limits.resends)
                        {
                            resendNode->type = NodeType::FinalResendNode;
                        }
                        else
                        {
                            resendNode->type = NodeType::NonfinalResendNode;
                        }
                    }

                    // Make the resend observation on the resend node
                    uint32_t arrivalDataOccurrences;
                    auto &savedArrivalData = addArrivalObservation(resendNode->arrivalDataAfterResend, arrivalData, &arrivalDataOccurrences);

                    // Jump out unless we need to explore more resends from here
                    if (resendNode->type != NodeType::NonfinalResendNode)
                    {
                        if (resendNode->type != NodeType::PoorResendNode)
                        {
                            // For gather paths, compute resendAlwaysArrivesDelta the first time
                            if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
                            {
                                if (arrivalDataOccurrences == 1)
                                {
                                    uint8_t successfulDelta = 0;
                                    for (int lastResendFrame = frame + simulatedPath.size() - 1; lastResendFrame > frame; lastResendFrame--)
                                    {
                                        if (resendFrames.contains(lastResendFrame - BWAPI::Broodwar->getLatencyFrames()))
                                        {
                                            successfulDelta++;
                                            continue;
                                        }

                                        resendFrames.insert(lastResendFrame);
                                        auto result = simWorker->simulateGatherPath(
                                                BWAPI::SimulateGatherPathOptions(resendFrames, initialState).setForceAction(true));
                                        resendFrames.erase(lastResendFrame);

                                        if (!result)
                                        {
                                            Log::Get() << "ERROR: Path could not be simulated";
                                            return;
                                        }

                                        if (result->positions.size() > 11) break;
                                        successfulDelta++;
                                    }

                                    // If it succeeds already from the first position, set to a placeholder to indicate such
                                    if (successfulDelta == (simulatedPath.size() - 1))
                                    {
                                        savedArrivalData.resendAlwaysArrivesDelta = (UINT8_MAX - 1);
                                    }
                                    else
                                    {
                                        savedArrivalData.resendAlwaysArrivesDelta = successfulDelta;
                                    }
                                }
                            }

                            addResult();
                        }
                        return;
                    }
                }

                // Loop through the path, creating and updating nodes as needed
                unsigned int resendAlwaysArrivesDelta = 0;
                for (auto positionIt = simulatedPath.begin(); positionIt != simulatedPath.end(); positionIt++)
                {
                    // The arrival delay is the distance to the last position node, which is the arrival position
                    auto arrivalDelay = std::distance(positionIt, simulatedPath.end()) - 1;

                    // We skip adding the last position, since we don't need it for optimization
                    if (arrivalDelay == 0) break;

                    frame++;

                    auto &position = *positionIt;
                    auto node = getNextPathNode(*nextPositions, position, true);
                    currentPosition = position;

                    // For new nodes, set the type if we can already determine it here
                    if (node->type == NodeType::Uninitialized)
                    {
                        if (arrivalDelay < limits.endOfExplorationWindow)
                        {
                            node->type = NodeType::AfterExplorationWindow;
                        }
                        else if ((frame - startFrame) < BWAPI::Broodwar->getLatencyFrames()
                                 || resendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames()))
                        {
                            node->type = NodeType::ResendUnavailable;
                        }
                    }

                    // Make the observation on the node
                    arrivalData.setArrivalDelay(arrivalDelay);
                    addArrivalObservation(node->arrivalData, arrivalData);

                    // If a resend is relevant from the node, explore one level deeper
                    // We check stable nodes 3 times since we do see some false positives
                    if (node->type == NodeType::Uninitialized || node->type == NodeType::NonfinalResendNode || node->type == NodeType::FinalResendNode
                        || (node->type == NodeType::StableNode && getTotalOccurrences(node->arrivalData) < 3))
                    {
                        std::set<int> nextResendFrames = resendFrames;
                        nextResendFrames.insert(frame);
                        explorePath(explorePath,
                                    frame,
                                    currentPosition,
                                    &node->nextPositionsAfterResend,
                                    std::move(nextResendFrames),
                                    node,
                                    std::ranges::subrange(positionIt + 1, simulatedPath.end()));
                    }
                    else if (node->type == NodeType::StableNode)
                    {
                        // For stable nodes, add a result as if we resent here, as it affects the order timer at arrival and therefore may differ from
                        // the initial node
                        std::set<int> nextResendFrames = resendFrames;
                        nextResendFrames.insert(frame);
                        results.emplace_back(arrivalData,
                                             simulatedPathWithDeliveryAtArrivalResult->nextPathStartPosition,
                                             simulatedPathWithDeliveryAtArrivalResult->actionFrame,
                                             simulatedPathWithDeliveryAtArrivalResult->lastOrderProcessTimerOverrideFrame,
                                             simulatedPathWithDeliveryAfterArrivalResult->nextPathStartPosition,
                                             simulatedPathWithDeliveryAfterArrivalResult->actionFrame,
                                             simulatedPathWithDeliveryAfterArrivalResult->lastOrderProcessTimerOverrideFrame,
                                             std::move(nextResendFrames));
                    }

                    if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
                    {
                        if (node->arrivalDataAfterResend.empty())
                        {
                            ++resendAlwaysArrivesDelta;
                        }
                        else
                        {
                            unsigned int maxArrivalDelay = 0;
                            for (const auto &[resendArrivalData, _] : node->arrivalDataAfterResend)
                            {
                                maxArrivalDelay = std::max(maxArrivalDelay, resendArrivalData.arrivalDelay());
                            }
                            if (maxArrivalDelay > 11)
                            {
                                resendAlwaysArrivesDelta = 0;
                            }
                            else
                            {
                                ++resendAlwaysArrivesDelta;
                            }
                        }

                        if (arrivalDelay == 1)
                        {
                            for (auto &[savedArrivalData, _] : node->arrivalData)
                            {
                                if (resendAlwaysArrivesDelta == (simulatedPath.size() - 1))
                                {
                                    savedArrivalData.resendAlwaysArrivesDelta = (UINT8_MAX - 1);
                                }
                                else
                                {
                                    savedArrivalData.resendAlwaysArrivesDelta = resendAlwaysArrivesDelta;
                                }
                            }
                        }
                    }

                    nextPositions = &node->nextPositions;
                }

                // Add the result if we did not resend after this node
                addResult();
            };

            // Start exploring the path from the worker's initial position
            explorePath(explorePath,
                        startFrame,
                        startPosition,
                        &rootNode.nextPositions[cannonPlacement]);
        };

        for (auto &[cannonPlacement, state]
                : patchToCannonsToStateCopy[startPosition.patch->getTilePosition()])
        {
            auto preparedReturnPath = prepareReturnPath(startPosition, *state);
            if (!preparedReturnPath) return;

            // Start by simulating the return path and gathering all results
            std::vector<NodeExplorationResult<ReturnArrivalData>> returnResults;
            makePathObservations({RETURN_EXPLORATION_WINDOW_START, RETURN_EXPLORATION_WINDOW_END, RETURN_RESEND_LIMIT},
                                 cannonPlacement,
                                 createReturnArrivalData,
                                 returnData,
                                 preparedReturnPath->startFrame,
                                 preparedReturnPath->state,
                                 preparedReturnPath->startPosition,
                                 returnResults);

            auto findUniqueNextPathStartPositions =
                    []<typename ObservationType>(std::vector<NodeExplorationResult<ObservationType>> &results,
                                                 int pathStartFrame)
                    {
                        std::set<NodeExplorationResult<ObservationType>*> bestResults;
                        auto findBestResults = [&](
                                std::optional<int> orderProcessTimerResetFrame = std::nullopt)
                        {
                            std::vector<std::tuple<NodeExplorationResult<ObservationType>*, int, int>> resultsWithActionFrameAndDelay;
                            int bestActionFrame = INT_MAX;
                            int bestActionFrameAndDelay = INT_MAX;
                            for (auto &result : results)
                            {
                                auto [actionFrame, delay] = result.arrivalData.computeActionFrame(pathStartFrame,
                                                                                                  result.resends.empty()
                                                                                                  ? std::nullopt
                                                                                                  : (std::optional<int>)*result.resends.rbegin(),
                                                                                                  orderProcessTimerResetFrame);
                                resultsWithActionFrameAndDelay.emplace_back(&result, actionFrame, delay);
                                bestActionFrame = std::min(bestActionFrame, actionFrame);
                                bestActionFrameAndDelay = std::min(bestActionFrameAndDelay, actionFrame + delay);
                            }

                            for (auto &[result, actionFrame, delay] : resultsWithActionFrameAndDelay)
                            {
                                if (actionFrame <= bestActionFrame || (actionFrame + delay) <= bestActionFrameAndDelay)
                                {
                                    bestResults.insert(result);
                                }
                            }

                            return bestActionFrame;
                        };

                        // Start without a reset
                        int bestNoResetActionFrame = findBestResults();

                        // Find the lower bound for what resets are interesting to explore
                        int maxLastResendFrame = currentFrame;
                        for (auto bestResult : bestResults)
                        {
                            if (bestResult->resends.empty()) continue;
                            maxLastResendFrame = std::max(maxLastResendFrame, *bestResult->resends.rbegin());
                        }

                        // Add all the best results at each reset frame
                        for (int resetFrame = maxLastResendFrame + 1; resetFrame <= bestNoResetActionFrame; resetFrame++)
                        {
                            findBestResults(resetFrame);
                        }

                        // Add the no-resend result since we might end up on that path accidentally
                        for (auto &result : results)
                        {
                            if (result.resends.empty()) bestResults.insert(&result);
                        }

                        // Now break this down to the set of unique next path start positions we want to explore gather paths for
                        // We only explore each exact position once, and skip positions that are already fully explored
                        std::map<BWAPI::ExactPosition, NodeExplorationResult<ObservationType>*> uniqueNextPathStartPositions;
                        for (auto bestResult : bestResults)
                        {
                            auto processNextPathStartPosition = [&](BWAPI::ExactPosition &nextPathStartPosition)
                            {
                                if (uniqueNextPathStartPositions.contains(nextPathStartPosition)) return;
                                uniqueNextPathStartPositions[nextPathStartPosition] = bestResult;
                            };
                            processNextPathStartPosition(bestResult->nextPathStartPositionActionAtArrival);
                            processNextPathStartPosition(bestResult->nextPathStartPositionActionAfterArrival);
                        }

                        return uniqueNextPathStartPositions;
                    };

            // Now perform path observations on each unique gather path start position
            for (auto &[gatherStartPosition, returnResult] : findUniqueNextPathStartPositions(returnResults, preparedReturnPath->startFrame))
            {
                // Simulate once to get the state copy at the start of the path
                auto result = simWorker->simulateGatherPath(
                        BWAPI::SimulateGatherPathOptions(returnResult->resends, preparedReturnPath->state)
                                .setForceAction(gatherStartPosition != returnResult->nextPathStartPositionActionAfterArrival)
                                .setReturnStateAtStartOfNextPath());
                if (!result)
                {
                    Log::Get() << "ERROR: Path could not be simulated";
                    return;
                }

                std::vector<NodeExplorationResult<GatherArrivalData>> gatherResults;
                makePathObservations({GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END, GATHER_RESEND_LIMIT},
                                     cannonPlacement,
                                     createGatherArrivalData,
                                     gatherData,
                                     result->actionFrame,
                                     result->stateAtStartOfNextPath,
                                     result->nextPathStartPosition,
                                     gatherResults);

                // Find all the best return start positions and add more start positions if applicable
                for (auto &[returnStartPosition, _] : findUniqueNextPathStartPositions(gatherResults, result->actionFrame))
                {
                    auto positionAndVelocity = PositionAndVelocity(returnStartPosition);
                    if (!returnData.contains(positionAndVelocity))
                    {
                        auto newPath = Path<ReturnArrivalData>{positionAndVelocity};
                        newPath.populatePositionsToExplore();
                        for (auto it = newPath.positionsToExplore.rbegin(); it != newPath.positionsToExplore.rend(); it++)
                        {
                            startPositions.emplace_back(ExploreStartPosition{*it, patch});
                        }
                        returnData[positionAndVelocity] = std::move(newPath);
                    }
                }
            }
        }
    }
}
