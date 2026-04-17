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
            std::set<int> resends;
        };
    }

    template <>
    void ExploreStartPositionsModule<ExploreInitialWorkerStartPosition>::initializeStartPositions()
    {
        for (const auto &[spawnPosition, _] : initialWorkerMapData.startingWorkerPositionToOrderProcessTimerReset)
        {
            auto base = Map::baseNear(spawnPosition);
            if (!base)
            {
                Log::Get() << "ERROR: Unexpectedly couldn't get a base for spawn position " << spawnPosition;
                return;
            }

            auto exactPosition = BWAPI::ExactPosition{
                    (uint32_t)spawnPosition.x * 256,
                    (uint32_t)spawnPosition.y * 256,
                    0, 0, 0
            };
            for (int heading = INT8_MIN; heading <= INT8_MAX; heading += 8)
            {
                exactPosition.heading = (int8_t)heading;

                for (auto &patch : base->mineralPatches())
                {
                    auto patchTile = TilePosition::fromBWAPI(patch->tile);
                    if (initialWorkerMapData.startingWorkerPositionToPatchToFirstGatherPath[exactPosition].contains(patchTile))
                    {
                        continue;
                    }

                    startPositions.emplace_back(ExploreInitialWorkerStartPosition{exactPosition, patch->getBwapiUnitIfVisible()});
                }
            }
        }
    }

    template <>
    void ExploreStartPositionsModule<ExploreInitialWorkerStartPosition>::explore(ExploreInitialWorkerStartPosition &startPosition)
    {
        // This is similar to the exploration done for normal gathering, but the scope is different because we know the subpixel positions and the
        // possible order process timer reset values the workers will encounter.

        // Find the base corresponding to this start position
        auto base = Map::baseNear(startPosition.pos.pos());
        if (!base)
        {
            Log::Get() << "ERROR: Unexpectedly couldn't get a base for start position " << startPosition.pos;
            return;
        }

        // Prepare the state so that the worker is at this start position
        auto prepareResult = simWorker->prepareGatherPath(
                BWAPI::PrepareGatherPathOptions(startPosition.pos, initialStateWithNoCannons.state));
        if (!prepareResult)
        {
            Log::Get() << "ERROR: Failed to prepare gather path for start position " << startPosition.pos;
            return;
        }

        auto makePathObservations = [&]<typename ObservationType>(
                const Limits &limits,
                InitialWorkerPathNode<ObservationType> &rootNode,
                int startFrame,
                std::unique_ptr<bwgame::state> &initialState,
                std::vector<NodeExplorationResult<ObservationType>> &results,
                BWAPI::Unit gatherPatch = nullptr)
        {
            // Explores the path, recursively going down a level as appropriate
            auto explorePath = [&]( // NOLINT(*-no-recursion)
                    auto &explorePath,
                    int frame,
                    std::unique_ptr<InitialWorkerPathNode<ObservationType>> *nextNode,
                    std::set<int> resendFrames = {},
                    InitialWorkerPathNode<ObservationType> *resendNode = nullptr,
                    std::ranges::subrange<std::vector<BWAPI::ExactPosition>::iterator> noResendPath = {})
            {
                // Simulate the path
                auto simulate = [&](bool forceAction)
                {
                    auto simulateOptions =
                            BWAPI::SimulateGatherPathOptions(resendFrames, initialState)
                                    .setForceAction(forceAction);
                    if (gatherPatch) simulateOptions.switchToPatch(gatherPatch->getBWIndex());
                    return simWorker->simulateGatherPath(simulateOptions);
                };

                std::unique_ptr<BWAPI::SimulateGatherPathResult> simulatedPathWithDeliveryAtArrivalResult = nullptr;
                std::unique_ptr<BWAPI::SimulateGatherPathResult> simulatedPathWithDeliveryAfterArrivalResult = nullptr;
                std::function<ObservationType()> createArrivalData;
                if constexpr (std::is_same_v<ObservationType, InitialWorkerGatherArrivalData>)
                {
                    createArrivalData = [&]()
                    {
                        simulatedPathWithDeliveryAtArrivalResult = simulate(true);
                        if (!simulatedPathWithDeliveryAtArrivalResult)
                        {
                            Log::Get() << "ERROR: Path could not be simulated";
                        }

                        return InitialWorkerGatherArrivalData::createFromSimulatedPath(*simulatedPathWithDeliveryAtArrivalResult, gatherPatch);
                    };
                }
                else
                {
                    createArrivalData = [&]()
                    {
                        simulatedPathWithDeliveryAtArrivalResult = simulate(true);
                        simulatedPathWithDeliveryAfterArrivalResult = simulate(false);
                        if (!simulatedPathWithDeliveryAtArrivalResult || !simulatedPathWithDeliveryAfterArrivalResult)
                        {
                            Log::Get() << "ERROR: Path could not be simulated";
                        }

                        return InitialWorkerReturnArrivalData::createFromSimulatedPath(*simulatedPathWithDeliveryAtArrivalResult,
                                                                                       *simulatedPathWithDeliveryAfterArrivalResult);
                    };
                }

                ObservationType arrivalData = createArrivalData();
                auto &simulatedPath = simulatedPathWithDeliveryAtArrivalResult->positions;

                auto addResult = [&]()
                {
                    // Reset the arrival delay since it might have been updated while exploring
                    arrivalData.arrivalDelay = simulatedPath.size();

                    // This is always called last, so we can use move semantics
                    results.emplace_back(std::move(arrivalData),
                                         std::move(resendFrames));
                };

                // If this is a resend node, check if the path changed
                if (resendNode)
                {
                    // If this node is uninitialized, set the node type
                    if (resendNode->type == NodeType::Uninitialized)
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

                    resendNode->arrivalDataAfterResend = arrivalData;

                    // Jump out unless we need to explore more resends from here
                    if (resendNode->type != NodeType::NonfinalResendNode)
                    {
                        if (resendNode->type != NodeType::PoorResendNode)
                        {
                            addResult();
                        }
                        return;
                    }
                }

                // Loop through the path and create the nodes
                for (auto positionIt = simulatedPath.begin(); positionIt != simulatedPath.end(); positionIt++)
                {
                    // The arrival delay is the distance to the last position node, which is the arrival position
                    auto arrivalDelay = std::distance(positionIt, simulatedPath.end()) - 1;

                    // We skip adding the last position, since we don't need it for optimization
                    if (arrivalDelay == 0) break;

                    frame++;

                    auto &position = *positionIt;
                    if (*nextNode)
                    {
                        (*nextNode)->pos = position;
                    }
                    else
                    {
                        (*nextNode) = std::make_unique<InitialWorkerPathNode<ObservationType>>(position);
                    }
                    auto &currentNode = **nextNode;

                    // Set the type if we can already determine it here
                    if (arrivalDelay < limits.endOfExplorationWindow)
                    {
                        currentNode.type = NodeType::AfterExplorationWindow;
                    }
                    else if ((frame - startFrame) < BWAPI::Broodwar->getLatencyFrames()
                             || resendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames()))
                    {
                        currentNode.type = NodeType::ResendUnavailable;
                    }

                    // Make the observation on the node
                    arrivalData.arrivalDelay = arrivalDelay;
                    currentNode.arrivalData = arrivalData;

                    // Explore resends on nodes that haven't already been classified
                    if (currentNode.type == NodeType::Uninitialized)
                    {
                        std::set<int> nextResendFrames = resendFrames;
                        nextResendFrames.insert(frame);
                        explorePath(explorePath,
                                    frame,
                                    &currentNode.nextPositionAfterResend,
                                    std::move(nextResendFrames),
                                    &currentNode,
                                    std::ranges::subrange(positionIt + 1, simulatedPath.end()));
                    }

                    nextNode = &currentNode.nextPosition;
                }

                // Add the result if we did not resend after this node
                addResult();
            };

            // Start exploring the path from the worker's initial position
            std::unique_ptr<InitialWorkerPathNode<ObservationType>> rootNodePtr(&rootNode);
            explorePath(explorePath,
                        startFrame,
                        &rootNodePtr);
            rootNodePtr.release();
        };

        // Start by simulating the first gather path and gathering all results
        auto result = initialWorkerMapData.startingWorkerPositionToPatchToFirstGatherPath[startPosition.pos]
                                                   .emplace(TilePosition::fromBWAPI(startPosition.patch->getTilePosition()), startPosition.pos);
        auto &firstGatherRootNode = result.first->second;
        std::vector<NodeExplorationResult<InitialWorkerGatherArrivalData>> gatherResults;
        makePathObservations({GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END, GATHER_RESEND_LIMIT},
                             firstGatherRootNode,
                             prepareResult->startFrame,
                             prepareResult->state,
                             gatherResults,
                             startPosition.patch);
    }
}
