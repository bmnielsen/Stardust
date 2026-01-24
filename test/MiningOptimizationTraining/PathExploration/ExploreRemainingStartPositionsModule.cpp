#include "ExploreRemainingStartPositionsModule.h"

#include "BWAPI/PrepareGatherPathOptions.h"
#include "BWAPI/PrepareGatherPathResult.h"
#include "BWAPI/SimulateGatherPathOptions.h"
#include "BWAPI/SimulateGatherPathResult.h"

#include "MiningOptimizationTraining/DataModel/Configuration.h"
#include "MiningOptimizationTraining/DataModel/Serialization.h"
#include "PathExplorationUtils.h"

#include "Map.h"

#include <chrono>

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

    bool ExploreRemainingStartPositionsModule::initialize()
    {
        if (executed) return false;

        pathsToExplore.clear();

        // Gather each test case we have remaining
        for (auto base : Map::allBases())
        {
            if (options.oneBase && base->getTilePosition() != options.oneBase) continue;

            for (auto &patch : base->mineralPatches())
            {
                if (options.onePatch && patch->tile != options.onePatch) continue;

                auto unit = patch->getBwapiUnitIfVisible();

                for (auto &[_, path] :
                    mapData.resourceToReturnPaths[TilePosition(patch->tile.x, patch->tile.y)])
                {
                    for (auto it = path.positionsToExplore.rbegin(); it != path.positionsToExplore.rend(); it++)
                    {
                        pathsToExplore.emplace_back(unit, *it, path);
                    }
                }
            }
        }

        Log::Get() << "Initialized; " << pathsToExplore.size() << " path(s) to explore";
        return true;
    }

    void ExploreRemainingStartPositionsModule::run()
    {
        if (executed) return;

        auto startCount = pathsToExplore.size();
        auto startTime = std::chrono::high_resolution_clock::now();
        long long lastOutput = 0;
        long long lastSaved = 0;

        while (!pathsToExplore.empty())
        {
            auto &current = pathsToExplore.front();

            explore(current);

            // Remove the position from the positions to explore
            // We expect it to always be the last one in the vector
            if (*current.path.positionsToExplore.rbegin() != current.startPosition)
            {
                Log::Get() << "ERROR: Position being processed is not last in the pending positions vector";
                return;
            }
            current.path.positionsToExplore.pop_back();
            pathsToExplore.pop_front();

            // Output status every 5 seconds
            long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - startTime).count();
            if (elapsed - lastOutput >= 5)
            {
                Log::Get() << "Processed " << (startCount - pathsToExplore.size()) << " path(s) in " << elapsed << " second(s); "
                           << pathsToExplore.size() << " remaining";
                lastOutput = elapsed;
            }

            // Save the map data every minute
            if (elapsed - lastSaved >= 60)
            {
                Serialization::writeMapData(mapData);
                lastSaved = elapsed;
            }
        }

        Log::Get() << "Done; processed " << startCount << " path(s) in "
                   << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - startTime).count()
                   << " second(s)";

        executed = true;
    }

    void ExploreRemainingStartPositionsModule::explore(PathToExplore &pathToExplore)
    {
        // Prepare the simulation by moving the sim worker to the start position
        auto prepareResult = simWorker->prepareGatherPath(
                BWAPI::PrepareGatherPathOptions(pathToExplore.startPosition, pathToExplore.patch->getBWIndex(), initialState.state));
        if (!prepareResult)
        {
            Log::Get() << "ERROR: Failed to prepare gather path";
            return;
        }
        if (prepareResult->returnPathStartPosition != pathToExplore.startPosition)
        {
            Log::Get() << "ERROR: Prepared gather path has incorrect start position";
            return;
        }

        auto &patch = pathToExplore.patch;

//        auto createGatherArrivalData = [&](
//                auto &simulatedPathWithActionAtArrival,
//                auto &simulatedPathWithActionAfterArrival)
//        {
//            return GatherArrivalData::createFromSimulatedPaths(simulatedPathWithActionAtArrival, simulatedPathWithActionAfterArrival, patch);
//        };

        auto createReturnArrivalData = [&](
                auto &simulatedPathWithActionAtArrival,
                auto &simulatedPathWithActionAfterArrival)
        {
            return ReturnArrivalData::createFromSimulatedPaths(simulatedPathWithActionAtArrival, simulatedPathWithActionAfterArrival);
        };

        auto makePathObservations = [&]<typename ObservationType>(
                std::vector<NodeExplorationResult<ObservationType>> &results,
                const Limits &limits,
                auto &createArrivalData,
                std::unordered_map<PositionAndVelocity, Path<ObservationType>> &rootNodes,
                int startFrame,
                std::unique_ptr<bwgame::state> *initialState,
                BWAPI::ExactPosition startPosition)
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
                            BWAPI::SimulateGatherPathOptions(resendFrames, *initialState).setForceAction(forceAction));
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
                        createArrivalData(*simulatedPathWithDeliveryAtArrivalResult, *simulatedPathWithDeliveryAfterArrivalResult);

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
                                            continue;
                                        }

                                        resendFrames.insert(lastResendFrame);
                                        auto result =
                                                simWorker->simulateGatherPath(BWAPI::SimulateGatherPathOptions(resendFrames).setForceAction(true));
                                        resendFrames.erase(lastResendFrame);

                                        if (!result)
                                        {
                                            Log::Get() << "ERROR: Path could not be simulated";
                                            return;
                                        }

                                        if (result->positions.size() > 11) break;
                                        successfulDelta++;
                                    }
                                    savedArrivalData.resendAlwaysArrivesDelta = successfulDelta;
                                }
                            }

                            addResult();
                        }
                        return;
                    }
                }

                // Loop through the path, creating and updating nodes as needed
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

                    nextPositions = &node->nextPositions;
                }

                // Add the result if we did not resend after this node
                addResult();
            };

            // Start exploring the path from the worker's initial position
            explorePath(explorePath,
                        startFrame,
                        startPosition,
                        &rootNode.nextPositions);
        };

        // Start by simulating the return path and gathering all results
        std::vector<NodeExplorationResult<ReturnArrivalData>> returnResults;
        makePathObservations(returnResults,
                             {RETURN_EXPLORATION_WINDOW_START, RETURN_EXPLORATION_WINDOW_END, RETURN_RESEND_LIMIT},
                             createReturnArrivalData,
                             mapData.resourceToReturnPaths[TilePosition::fromBWAPI(patch->getTilePosition())],
                             prepareResult->returnPathStartFrame,
                             &prepareResult->returnPathState,
                             prepareResult->returnPathStartPosition);

    }
}
