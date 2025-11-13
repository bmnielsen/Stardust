#include "WorkerPathExploration.h"
#include "../DataModel/Configuration.h"

#include <ranges>

#define EPSILON 0.0001

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
                                                   BWAPI::ExactPositionDifference positionDifference,
                                                   bool update)
        {
            std::pair<PathNode<ObservationType>, uint32_t> *nextPathNodePair = nullptr;
            for (auto &pathNodePair : nextPositions)
            {
                if (pathNodePair.first.positionDifferenceFromPreviousNode == positionDifference)
                {
                    nextPathNodePair = &pathNodePair;
                    break;
                }
            }

            if (!update && !nextPathNodePair) return nullptr;

            if (!nextPathNodePair)
            {
                nextPathNodePair = &nextPositions.emplace_back(PathNode<ObservationType>{positionDifference}, 0);
            }

            if (update && getTotalOccurrences(nextPositions) < UINT32_MAX)
            {
                nextPathNodePair->second++;
            }

            return &(nextPathNodePair->first);
        }

        // Adds an arrival observation to the given observations map
        template <typename ObservationType>
        void addArrivalObservation(std::map<ObservationType, uint32_t> &observations, const ObservationType &arrivalData)
        {
            auto dataIt = observations.find(arrivalData);
            if (dataIt == observations.end())
            {
                dataIt = observations.emplace(arrivalData, 0).first;
            }

            if (getTotalOccurrences(observations) < UINT32_MAX) dataIt->second++;
        }

        // Gets the number of times a root node has been explored
        template <typename ObservationType>
        uint32_t getTimesExplored(const std::unordered_map<PositionAndVelocity, Path<ObservationType>> &pathRootNodes,
                                  const PositionAndVelocity &pos)
        {
            auto it = pathRootNodes.find(pos);
            if (it == pathRootNodes.end()) return 0;
            return it->second.timesExplored;
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
            std::pair<unsigned int, int> arrivalDelay;
            BWAPI::ExactPosition nextPathStartPosition;
            std::set<int> resends;

            NodeExplorationResult(ObservationType &arrivalData, BWAPI::ExactPosition nextPathStartPosition, std::set<int> resends)
                    : arrivalDelay(arrivalData.calculateFullDelay(resends.empty() ? 0 : (*resends.rbegin() - currentFrame)))
                    , nextPathStartPosition(std::move(nextPathStartPosition))
                    , resends(std::move(resends))
            {}
        };
    }

    void WorkerPathExploration::update()
    {
        auto createGatherArrivalData = [&](auto &simulatedPath)
        {
            return GatherArrivalData::createFromSimulatedPath(simulatedPath, patch);
        };

        auto createReturnArrivalData = [&](auto &simulatedPath)
        {
            return ReturnArrivalData::createFromSimulatedPath(simulatedPath);
        };

        // Called on the first position of a path to make the observations and plan the resends
        // Observations are done through the path simulation, so don't require actual resends in the game
        // Resends are planned to get us to the least-explored root node of the next path
        auto initializePath = [&]<typename ObservationType, typename NextObservationType>(
                const Limits &limits,
                auto &createArrivalData,
                std::unordered_map<PositionAndVelocity, Path<ObservationType>> &rootNodes,
                std::unordered_map<PositionAndVelocity, Path<NextObservationType>> &nextPathRootNodes
            )
        {
            plannedResendFrames.clear();

            // Get or create the root node
            auto currentPositionAndVelocity = PositionAndVelocity(worker);
            auto rootNodeIt = rootNodes.find(currentPositionAndVelocity);
            if (rootNodeIt == rootNodes.end())
            {
                rootNodeIt = rootNodes.emplace(currentPositionAndVelocity, Path<ObservationType>{currentPositionAndVelocity}).first;
            }
            auto &rootNode = rootNodeIt->second;
            rootNode.timesExplored++;

            // While exploring, we gather the results of each subpath in order to pick a good next path to explore and keep some overall statistics
            std::vector<NodeExplorationResult<ObservationType>> results;

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
                auto simulatedPathResult = worker->simulateGatherPath(resendFrames);
                if (!simulatedPathResult.has_value())
                {
                    Log::Get() << "ERROR: Path could not be simulated"
                               << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                    return;
                }
                auto &simulatedPath = std::get<0>(*simulatedPathResult);
                auto &nextPathStartPos = std::get<1>(*simulatedPathResult);
                ObservationType arrivalData = createArrivalData(*simulatedPathResult);

                auto addResult = [&]()
                {
                    // Reset the arrival delay since it might have been updated while exploring
                    arrivalData.setArrivalDelay(simulatedPath.size());

                    // This is always called last, so we can use move semantics
                    results.emplace_back(arrivalData, std::move(nextPathStartPos), std::move(resendFrames));
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
                    addArrivalObservation(resendNode->arrivalDataAfterResend, arrivalData);

                    // Jump out unless we need to explore more resends from here
                    if (resendNode->type != NodeType::NonfinalResendNode)
                    {
                        if (resendNode->type != NodeType::PoorResendNode) addResult();
                        return;
                    }
                }

                // Loop through the path, creating and updating nodes as needed
                for (auto positionIt = simulatedPath.begin(); positionIt != simulatedPath.end(); positionIt++)
                {
                    auto &position = *positionIt;
                    auto node = getNextPathNode(*nextPositions, position - currentPosition, true);
                    currentPosition = position;

                    // The arrival delay is the distance to the last position node, which is the arrival position
                    auto arrivalDelay = std::distance(positionIt, simulatedPath.end());

                    // For new nodes, set the type if we can already determine it here
                    if (node->type == NodeType::Uninitialized)
                    {
                        if (arrivalDelay < limits.endOfExplorationWindow)
                        {
                            node->type = NodeType::AfterExplorationWindow;
                        }
                        else if ((frame - currentFrame) < BWAPI::Broodwar->getLatencyFrames()
                            || resendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames()))
                        {
                            node->type = NodeType::ResendUnavailable;
                        }
                    }

                    // Make the observation on the node
                    if (node->type != NodeType::AfterExplorationWindow)
                    {
                        arrivalData.setArrivalDelay(arrivalDelay);
                        addArrivalObservation(node->arrivalData, arrivalData);
                    }

                    // If a resend is relevant from the node, explore one level deeper
                    // We check stable nodes 3 times since we do see some false positives
                    if (node->type == NodeType::Uninitialized || node->type == NodeType::NonfinalResendNode || node->type == NodeType::FinalResendNode
                        || (node->type == NodeType::StableNode && getTotalOccurrences(node->arrivalData) < 3))
                    {
                        std::set<int> nextResendFrames = resendFrames;
                        nextResendFrames.insert(frame);
                        explorePath(explorePath,
                                    frame + 1,
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
                        results.emplace_back(arrivalData, nextPathStartPos, std::move(nextResendFrames));
                    }

                    nextPositions = &node->nextPositions;
                    frame++;
                }

                // Add the result if we did not resend after this node
                addResult();
            };

            // Start exploring the path from the worker's initial position
            explorePath(explorePath,
                        currentFrame,
                        worker->getExactPosition(),
                        &rootNode.nextPositions);

            // Process the results
            // The goals are twofold:
            // - Keep statistics on the root node of how the optimal arrival delays tend to shake out, which we can later use to prioritize paths
            //   that get us to better next paths.
            // - Plan to explore relevant next paths, excluding those that will never come up in real situations because they only occur after
            //   horrible earlier path decisions. This especially applies to return paths that are not being straightened.

            // Start by scanning to get the best arrival data
            unsigned int bestArrivalDelay;
            for (auto &result : results)
            {
                unsigned int arrivalDelay = (result.arrivalDelay.first + result.arrivalDelay.second);
                if (arrivalDelay < bestArrivalDelay)
                {
                    bestArrivalDelay = arrivalDelay;
                }
            }
            if (getTotalOccurrences(rootNode.bestArrivalDelaysAndOccurrences) < UINT32_MAX)
            {
                rootNode.bestArrivalDelaysAndOccurrences[bestArrivalDelay]++;
            }

            // Now choose the next path start node we want to explore
            // Our scoring function is to take the times explored and weight it by the difference between the arrival delay and the best arrival delay
            // The score is doubled for every 5 frames of additional delay
            auto bestScore = (float)UINT32_MAX;
            unsigned int bestDelayDelta = UINT_MAX;
            for (auto &result : results)
            {
                float score = getTimesExplored(nextPathRootNodes, PositionAndVelocity(result.nextPathStartPosition));
                unsigned int arrivalDelayDelta = (result.arrivalDelay.first + result.arrivalDelay.second) - bestArrivalDelay;
                score *= (1.0f + (float)arrivalDelayDelta / 5.0f);

                if (score < (bestScore - EPSILON) || (score < (bestScore + EPSILON) && arrivalDelayDelta < bestDelayDelta))
                {
                    bestScore = score;
                    bestDelayDelta = arrivalDelayDelta;
                    plannedResendFrames = std::move(result.resends);
#if VALIDATE_EXPECTED_TRANSITION_FRAMES
                    expectedTransitionFrame = currentFrame + result.arrivalDelay.first;
#endif
                }
            }

            std::ostringstream dbg;
            std::string sep;
            for (auto frame : plannedResendFrames)
            {
                dbg << sep << frame;
                sep = ", ";
            }
            CherryVis::log(worker->getID()) << "Planned resend frame(s): " << dbg.str();
#if VALIDATE_EXPECTED_TRANSITION_FRAMES
            CherryVis::log(worker->getID()) << "Expected transition frame: " << expectedTransitionFrame;
#endif
        };

        while (true)
        {
            switch (state)
            {
                case 0:
                {
                    // Worker is approaching the patch; transition to state 1 when it is waiting for minerals
                    if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
                    {
                        CherryVis::log(worker->getID()) << "State transition from approaching patch to wait for minerals";
                        state = 1;

#if VALIDATE_EXPECTED_TRANSITION_FRAMES
                        if (expectedTransitionFrame != -1 && expectedTransitionFrame != currentFrame)
                        {
                            Log::Get() << "ERROR: Incorrect expected transition frame of " << expectedTransitionFrame
                                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();

                        }
#endif
                        continue;
                    }

                    // Execute a desired resend
                    if (plannedResendFrames.contains(currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        CherryVis::log(worker->getID()) << "Issuing gather command";
                        auto result = worker->gather(patch);
                        if (!result)
                        {
                            Log::Get() << "ERROR: Failed to reissue gather command: " << BWAPI::Broodwar->getLastError()
                                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                        }
                    }

                    return;
                }
                case 1:
                {
                    // Worker is waiting for minerals; transition to state 2 when it starts mining
                    if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
                    {
                        CherryVis::log(worker->getID()) << "State transition from wait for minerals to mining";
                        state = 2;
                        continue;
                    }

                    return;
                }
                case 2:
                {
                    // Worker is mining; transition to state 2 when it is finished mining
                    if (worker->getOrder() == BWAPI::Orders::ReturnMinerals && worker->isCarryingMinerals())
                    {
                        CherryVis::log(worker->getID()) << "State transition from mining to returning minerals";
                        state = 3;
                        initializePath(
                                {RETURN_EXPLORATION_WINDOW_START, RETURN_EXPLORATION_WINDOW_END, RETURN_RESEND_LIMIT},
                                createReturnArrivalData,
                                returnPaths,
                                gatherPaths);
                        continue;
                    }

                    return;
                }
                case 3:
                {
                    // Worker is returning minerals; transition to state 0 when it has returned minerals
                    if (!worker->isCarryingMinerals())
                    {
                        CherryVis::log(worker->getID()) << "State transition from returning minerals to approaching patch";
                        state = 0;

#if VALIDATE_EXPECTED_TRANSITION_FRAMES
                        if (expectedTransitionFrame != -1 && expectedTransitionFrame != currentFrame)
                        {
                            Log::Get() << "ERROR: Incorrect expected transition frame of " << expectedTransitionFrame
                                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();

                        }
#endif

                        initializePath(
                                {GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END, GATHER_RESEND_LIMIT},
                                createGatherArrivalData,
                                gatherPaths,
                                returnPaths);
                        continue;
                    }

                    // Execute a desired resend
                    if (plannedResendFrames.contains(currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        CherryVis::log(worker->getID()) << "Issuing return command";
                        auto result = worker->rightClick(depot);
                        if (!result)
                        {
                            Log::Get() << "ERROR: Failed to reissue return command: " << BWAPI::Broodwar->getLastError()
                                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                        }
                    }

                    return;
                }
                default:
                {
                    Log::Get() << "ERROR: Worker has unknown state " << state;
                    return;
                }
            }
        }
    }

    void WorkerPathExploration::outputDebugInformation()
    {
        std::ostringstream dbg;
        dbg << patch->getTilePosition() << ": ";
        auto dbgPath = []<typename ObservationType>(std::unordered_map<PositionAndVelocity, Path<ObservationType>> &paths)
        {
            std::ostringstream out;
            out << paths.size() << " path(s)";
            if (!paths.empty())
            {
                uint32_t mostExplored = 0;
                uint32_t leastExplored = UINT32_MAX;
                uint64_t totalExplored = 0;
                for (auto &[_, path] : paths)
                {
                    mostExplored = std::max(mostExplored, path.timesExplored);
                    leastExplored = std::min(leastExplored, path.timesExplored);
                    totalExplored += path.timesExplored;
                }
                out << "; most explored " << mostExplored
                    << "; least explored " << leastExplored
                    << "; avg explored " << (totalExplored / paths.size());
            }
            return out.str();
        };
        dbg << "Gather: " << dbgPath(gatherPaths);
        dbg << "; Return: " << dbgPath(returnPaths);
        Log::Get() << dbg.str();
    }
}
