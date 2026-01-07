#include "WorkerPathExploration.h"
#include "../DataModel/Configuration.h"

#include "BWAPI/SimulateGatherPathOptions.h"
#include "BWAPI/SimulateGatherPathResult.h"

#include "PathExplorationUtils.h"

#include <ranges>

#define EPSILON 0.0001

// The number of times the most-explored path (gather and return) should be explored for us to consider a patch "finished"
#define EXPLORATION_GOAL 150

// The number of times we explore a specific start node of a path, split on collision or not
#define ROOT_NODE_EXPLORATION_LIMIT 100

// How the score of a path is weighted based on the difference between its arrival delay and the best one

// This weighting is an exponential function that roughly doubles the score at 4 frames and multiplies by 10 at 10 frames
//#define EXPLORATION_SCORING_FACTOR 0.4f * std::pow(3.5f, (float)arrivalDelayDelta / 4.0f) + 0.6f

// This weighting doubles the score every 4 frames
#define EXPLORATION_SCORING_FACTOR (1.0f + ((float)arrivalDelayDelta / 4.0f))


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
            BWAPI::ExactPosition nextPathStartPositionActionAfterArrival;
            std::set<int> resends;
        };
    }

    void WorkerPathExploration::update()
    {
        auto createGatherArrivalData = [&](
                auto &simulatedPathWithActionAtArrival,
                auto &simulatedPathWithActionAfterArrival)
        {
            return GatherArrivalData::createFromSimulatedPaths(simulatedPathWithActionAtArrival, simulatedPathWithActionAfterArrival, patch);
        };

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
                int startFrame = currentFrame,
                BWAPI::SimulateGatherPathResult *previousPath = nullptr)
        {
            // For the first path, the worker's current position will be the starting point. For subsequent paths, we'll use the previous path result.
            auto currentExactPosition = previousPath ? previousPath->nextPathStartPosition : worker->getExactPosition();

            // Get or create the root node
            auto currentPositionAndVelocity = PositionAndVelocity(currentExactPosition);
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
                    // If we have a previous path, include the starting state
                    if (previousPath)
                    {
                        return worker->simulateGatherPath(
                                BWAPI::SimulateGatherPathOptions(resendFrames, previousPath->stateAtStartOfNextPath).setForceAction(forceAction));
                    }
                    return worker->simulateGatherPath(
                            BWAPI::SimulateGatherPathOptions(resendFrames).setForceAction(forceAction));
                };

                // We both simulate with action at arrival and action after arrival
                auto simulatedPathWithDeliveryAtArrivalResult = simulate(true);
                auto simulatedPathWithDeliveryAfterArrivalResult = simulate(false);
                if (!simulatedPathWithDeliveryAtArrivalResult || !simulatedPathWithDeliveryAfterArrivalResult)
                {
                    Log::Get() << "ERROR: Path could not be simulated"
                               << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
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
                                         std::move(simulatedPathWithDeliveryAfterArrivalResult->nextPathStartPosition),
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
                            // For gather paths, compute resendAlwaysArrivesDelta the first three times
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
                                                worker->simulateGatherPath(BWAPI::SimulateGatherPathOptions(resendFrames).setForceAction(true));
                                        resendFrames.erase(lastResendFrame);

                                        if (!result)
                                        {
                                            Log::Get() << "ERROR: Path could not be simulated"
                                                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
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
                                             simulatedPathWithDeliveryAfterArrivalResult->nextPathStartPosition,
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
                        currentExactPosition,
                        &rootNode.nextPositions);
        };

        // Called on the first position of a return path to make observations and plan resends
        // Observations are made on all return paths and subsequent gather paths through path simulation, so don't require actual resends in the game
        // Resends are planned to get us to the least-explored root node of the next return path
        auto processPath = [&]()
        {
            plannedResendFrames.clear();

            // Start by simulating the return path and gathering all results
            std::vector<NodeExplorationResult<ReturnArrivalData>> returnResults;
            makePathObservations(returnResults,
                                 {RETURN_EXPLORATION_WINDOW_START, RETURN_EXPLORATION_WINDOW_END, RETURN_RESEND_LIMIT},
                                 createReturnArrivalData,
                                 returnPaths);

            // Now compute the best results: results that give the optimal action frame considering different reset frames
            std::set<NodeExplorationResult<ReturnArrivalData>*> bestResults;
            auto findBestResults = [&](std::optional<int> orderProcessTimerResetFrame = std::nullopt)
            {
                std::set<NodeExplorationResult<ReturnArrivalData>*> theseBestResults;
                int bestActionFrame = INT_MAX;
                for (auto &result : returnResults)
                {
                    auto actionFrame = result.arrivalData.computeActionFrame(result.resends.empty()
                                                                             ? std::nullopt
                                                                             : (std::optional<int>)*result.resends.rbegin(),
                                                                             orderProcessTimerResetFrame);
                    if (actionFrame < bestActionFrame)
                    {
                        bestActionFrame = actionFrame;
                        theseBestResults.clear();
                    }
                    if (actionFrame == bestActionFrame)
                    {
                        theseBestResults.insert(&result);
                    }
                }
                bestResults.insert(theseBestResults.begin(), theseBestResults.end());
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

            // Now break this down to the set of unique next path start positions we want to explore gather paths for
            // We only explore each exact position once, and skip positions that are already fully explored
            std::map<BWAPI::ExactPosition, NodeExplorationResult<ReturnArrivalData>*> uniqueNextPathStartPositions;
            for (auto bestResult : bestResults)
            {
                auto processNextPathStartPosition = [&](BWAPI::ExactPosition &nextPathStartPosition, bool collision)
                {
                    if (uniqueNextPathStartPositions.contains(nextPathStartPosition)) return;
                    if (getTimesExplored(gatherPaths, PositionAndVelocity(nextPathStartPosition), collision) > ROOT_NODE_EXPLORATION_LIMIT)
                    {
                        return;
                    }
                    uniqueNextPathStartPositions[nextPathStartPosition] = bestResult;
                };
                processNextPathStartPosition(bestResult->nextPathStartPositionActionAtArrival,
                                             bestResult->arrivalData.isCollisionWithActionAtArrival());
                processNextPathStartPosition(bestResult->nextPathStartPositionActionAtArrival,
                                             bestResult->arrivalData.isCollisionWithActionAfterArrival());
            }

            CherryVis::log(worker->getID()) << "Performed return simulations; simulated " << returnResults.size() << " paths and have "
                                            << uniqueNextPathStartPositions.size() << " gather path start positions to simulate from";

            // If there are no root nodes at all left to explore, just pick the first one
            if (uniqueNextPathStartPositions.empty())
            {
                uniqueNextPathStartPositions[(*bestResults.begin())->nextPathStartPositionActionAtArrival] = *bestResults.begin();
            }

            // Simulate each gather path resulting from the positions in the map
            std::vector<std::tuple<BWAPI::ExactPosition,
                                   NodeExplorationResult<ReturnArrivalData> *,
                                   std::vector<NodeExplorationResult<GatherArrivalData>>>> gatherResults;
            for (auto &[startPosition, returnResult] : uniqueNextPathStartPositions)
            {
                // Simulate once to get the state copy at the start of the path
                auto result = worker->simulateGatherPath(BWAPI::SimulateGatherPathOptions(returnResult->resends)
                        .setForceAction(startPosition != returnResult->nextPathStartPositionActionAfterArrival)
                        .setReturnStateAtStartOfNextPath());
                if (!result)
                {
                    Log::Get() << "ERROR: Path could not be simulated"
                               << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                    return;
                }

                // Compute the start frame of the gather path
                int arrivalFrame = (returnResult->resends.empty() ? currentFrame : *returnResult->resends.rbegin())
                        + (int)returnResult->arrivalData.arrivalDelay();
                int actionFrame = (startPosition != returnResult->nextPathStartPositionActionAfterArrival) ? arrivalFrame : (arrivalFrame + 8);

                std::vector<NodeExplorationResult<GatherArrivalData>> returnPathGatherResults;
                makePathObservations(returnPathGatherResults,
                                     {GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END, GATHER_RESEND_LIMIT},
                                     createGatherArrivalData,
                                     gatherPaths,
                                     actionFrame,
                                     result.get());
                gatherResults.emplace_back(startPosition, returnResult, std::move(returnPathGatherResults));
            }

            // Find the least-explored return root node in all of the gather results and plan resends to get us there
            unsigned int leastExplored = UINT_MAX;
            for (auto &[startPosition, returnResult, returnPathGatherResults] : gatherResults)
            {
                for (auto &result : returnPathGatherResults)
                {
                    auto timesExplored = getTimesExplored(returnPaths,
                                                          PositionAndVelocity(result.nextPathStartPositionActionAtArrival),
                                                          result.arrivalData.collision());
                    if (timesExplored < leastExplored)
                    {
                        leastExplored = timesExplored;
                        plannedResendFrames = returnResult->resends;
                        plannedResendFrames.insert(result.resends.begin(), result.resends.end());

                        int arrivalFrame = (returnResult->resends.empty() ? currentFrame : *returnResult->resends.rbegin())
                                           + (int)returnResult->arrivalData.arrivalDelay();
                        plannedSetOrderProcessTimerFrame =
                                std::make_pair(arrivalFrame - 1,
                                               (startPosition == returnResult->nextPathStartPositionActionAfterArrival) ? 8 : 0);
                    }
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
        };

        auto stateChange = [&](int to)
        {
            CherryVis::log(worker->getID()) << "State transition from " << state << " to " << to;
            stateCount[state]++;
            state = to;
        };

        // We treat the worker as finished if both gather and return have exceeded our exploration goal
        auto finishedExploring = [&]()
        {
            auto fullyExplored = []<typename ObservationType>(const std::unordered_map<PositionAndVelocity, Path<ObservationType>> &paths)
            {
                for (auto &[_, path] : paths)
                {
                    if ((path.timesExploredWithCollision + path.timesExploredWithNoCollision) >= EXPLORATION_GOAL) return true;
                }
                return false;
            };

            if (fullyExplored(gatherPaths) && fullyExplored(returnPaths))
            {
                stateChange(4);
                BWAPI::Broodwar->killUnit(worker);
                return true;
            }

            return false;
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
                        stateChange(1);
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
                    else if (plannedSetOrderProcessTimerFrame.first == (currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        CherryVis::log(worker->getID()) << "Setting order process timer to " << plannedSetOrderProcessTimerFrame.second;
                        worker->setOrderProcessTimer(plannedSetOrderProcessTimerFrame.second);
                    }

                    return;
                }
                case 1:
                {
                    // Worker is waiting for minerals; transition to state 2 when it starts mining
                    if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
                    {
                        stateChange(2);
                        continue;
                    }

                    return;
                }
                case 2:
                {
                    // Worker is mining; transition to state 2 when it is finished mining
                    if (worker->getOrder() == BWAPI::Orders::ReturnMinerals && worker->isCarryingMinerals())
                    {
                        if (finishedExploring()) return;

                        stateChange(3);

                        if (stateCount[3] > 0)
                        {
                            processPath();
                        }
                        continue;
                    }

                    return;
                }
                case 3:
                {
                    // Worker is returning minerals; transition to state 0 when it has returned minerals
                    if (!worker->isCarryingMinerals())
                    {
                        if (finishedExploring()) return;

                        stateChange(0);
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
                    else if (plannedSetOrderProcessTimerFrame.first == (currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        CherryVis::log(worker->getID()) << "Setting order process timer to " << plannedSetOrderProcessTimerFrame.second;
                        worker->setOrderProcessTimer(plannedSetOrderProcessTimerFrame.second);
                    }

                    return;
                }
                case 4:
                {
                    // We've identified that we don't need to do any more work
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

    void WorkerPathExploration::outputDebugInformation() const
    {
        std::ostringstream dbg;
        dbg << patch->getTilePosition() << ": ";
        auto dbgPath = []<typename ObservationType>(const std::unordered_map<PositionAndVelocity, Path<ObservationType>> &paths)
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
                    mostExplored = std::max(mostExplored, (path.timesExploredWithCollision + path.timesExploredWithNoCollision));
                    leastExplored = std::min(leastExplored, (path.timesExploredWithCollision + path.timesExploredWithNoCollision));
                    totalExplored += (path.timesExploredWithCollision + path.timesExploredWithNoCollision);
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
