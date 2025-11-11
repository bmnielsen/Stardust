#include "WorkerPathExploration.h"
#include "../DataModel/Configuration.h"

#include <ranges>

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

        // Container for keeping track of the least-explored next path found so far
        struct LeastExploredNode
        {
            uint32_t timesExplored = UINT32_MAX;
            std::set<int> resends;

            void updateIfBetter(LeastExploredNode &other)
            {
                if (other.timesExplored < timesExplored
                    || (other.timesExplored == timesExplored && resends.size() > other.resends.size()))
                {
                    timesExplored = other.timesExplored;
                    resends = std::move(other.resends);
                }
            }
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

            // Explores the path, recursively going down a level as appropriate
            auto explorePath = [&]( // NOLINT(*-no-recursion)
                    auto &explorePath,
                    int frame,
                    BWAPI::ExactPosition currentPosition,
                    std::vector<std::pair<PathNode<ObservationType>, uint32_t>> *nextPositions,
                    std::set<int> &resendFrames,
                    PathNode<ObservationType> *resendNode = nullptr,
                    std::ranges::subrange<std::vector<BWAPI::ExactPosition>::iterator> noResendPath = {}) -> LeastExploredNode
            {
                // Simulate the path
                auto simulatedPathResult = worker->simulateGatherPath(resendFrames);
                if (!simulatedPathResult.has_value())
                {
                    Log::Get() << "ERROR: Path could not be simulated"
                               << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                    return {};
                }
                auto &simulatedPath = std::get<0>(*simulatedPathResult);
                auto &nextPathStartPos = std::get<1>(*simulatedPathResult);
                ObservationType arrivalData = createArrivalData(*simulatedPathResult);

                // Initialize the best result to the case where we follow the path with no additional resends
                LeastExploredNode bestResult{getTimesExplored(nextPathRootNodes, PositionAndVelocity(nextPathStartPos)), resendFrames};

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
                            return {};
                        }

                        // Set the type of resend node depending on whether we are at our resend depth limit
                        if (resendFrames.size() >= limits.resends)
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

                    // If we are at our depth limit, we don't need to explore the nodes along the path
                    if (resendNode->type == NodeType::FinalResendNode) return bestResult;
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
                        if ((frame - currentFrame) < BWAPI::Broodwar->getLatencyFrames()
                            || (arrivalDelay > limits.startOfExplorationWindow && resendFrames.empty()))
                        {
                            node->type = NodeType::BeforeExplorationWindow;
                        }
                        else if (arrivalDelay < limits.endOfExplorationWindow)
                        {
                            node->type = NodeType::AfterExplorationWindow;
                        }
                        else if (resendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames()))
                        {
                            node->type = NodeType::ResendUnavailable;
                        }
                    }

                    // Make the observation on the node
                    if (node->type != NodeType::BeforeExplorationWindow && node->type != NodeType::AfterExplorationWindow)
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
                        auto resendResult = explorePath(explorePath,
                                                        frame + 1,
                                                        currentPosition,
                                                        &node->nextPositionsAfterResend,
                                                        nextResendFrames,
                                                        node,
                                                        std::ranges::subrange(positionIt + 1, simulatedPath.end()));
                        bestResult.updateIfBetter(resendResult);
                    }

                    nextPositions = &node->nextPositions;
                    frame++;
                }

                return bestResult;
            };

            std::set<int> resendFrames;
            auto result = explorePath(explorePath,
                                      currentFrame,
                                      worker->getExactPosition(),
                                      &rootNode.nextPositions,
                                      resendFrames);
            plannedResendFrames = std::move(result.resends);

            std::ostringstream dbg;
            std::string sep;
            for (auto frame : plannedResendFrames)
            {
                dbg << sep << frame;
                sep = ", ";
            }
            CherryVis::log(worker->getID()) << "Planned resend frame(s): " << dbg.str();
        };

        switch (state)
        {
            case 0:
            {
                // Worker is approaching the patch; transition to state 1 when it starts mining
                if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    CherryVis::log(worker->getID()) << "State transition from approaching patch to mining";
                    state = 1;
                    return;
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
                // Worker is mining; transition to state 2 when it is finished mining
                if (worker->getOrder() == BWAPI::Orders::ReturnMinerals && worker->isCarryingMinerals())
                {
                    CherryVis::log(worker->getID()) << "State transition from mining to returning minerals";
                    state = 2;
                    initializePath(
                            {RETURN_EXPLORATION_WINDOW_START, RETURN_EXPLORATION_WINDOW_END, RETURN_RESEND_LIMIT},
                            createReturnArrivalData,
                            returnPaths,
                            gatherPaths);
                }

                return;
            }
            case 2:
            {
                // Worker is returning minerals; transition to state 0 when it has returned minerals
                if (!worker->isCarryingMinerals())
                {
                    CherryVis::log(worker->getID()) << "State transition from returning minerals to approaching patch";
                    state = 0;
                    initializePath(
                            {GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END, GATHER_RESEND_LIMIT},
                            createGatherArrivalData,
                            gatherPaths,
                            returnPaths);
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
