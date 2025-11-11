#include "WorkerPathExploration.h"
#include "../DataModel/Configuration.h"

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

        // Gets the next path node matching a specific next position
        // If update is set, nodes are created where they don't exist and occurrence counts are incremented
        template <typename ObservationType>
        PathNode<ObservationType> *getNextPathNode(std::vector<std::pair<PathNode<ObservationType>, uint32_t>> &nextPositions,
                                                   BWAPI::ExactPositionDifference positionDifference,
                                                   bool update)
        {
            uint32_t totalOccurrences = 0;
            std::pair<PathNode<ObservationType>, uint32_t> *nextPathNodePair = nullptr;
            for (auto &pathNodePair : nextPositions)
            {
                if (pathNodePair.first.positionDifferenceFromPreviousNode == positionDifference) nextPathNodePair = &pathNodePair;
                totalOccurrences += pathNodePair.second;
            }

            if (!update && !nextPathNodePair) return nullptr;

            if (!nextPathNodePair)
            {
                nextPathNodePair = &nextPositions.emplace_back(PathNode<ObservationType>{positionDifference}, 0);
            }

            if (update && totalOccurrences < UINT32_MAX)
            {
                nextPathNodePair->second++;
            }

            return &(nextPathNodePair->first);
        }

        // Adds an arrival observation to the given observations map
        template <typename ObservationType>
        void addArrivalObservation(std::map<ObservationType, uint32_t> &observations, const ObservationType &arrivalData)
        {
            uint32_t totalOccurrences = 0;
            for (const auto &[_, occurrences] : observations) totalOccurrences += occurrences;

            auto dataIt = observations.find(arrivalData);
            if (dataIt == observations.end())
            {
                dataIt = observations.emplace(arrivalData, 0).first;
            }

            if (totalOccurrences < UINT32_MAX) dataIt->second++;
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

        // Container for keeping track of the least-explored next path found so far
        struct LeastExploredNode
        {
            uint32_t timesExplored;
            std::set<int> resends;

            template <typename NextObservationType>
            void updateIfBetter(const std::unordered_map<PositionAndVelocity, Path<NextObservationType>> &nextPathRootNodes,
                                const PositionAndVelocity &otherNextPathStartPos,
                                std::set<int> otherResends)
            {
                uint32_t otherTimesExplored = getTimesExplored(nextPathRootNodes, otherNextPathStartPos);

                if (otherTimesExplored < timesExplored
                    || (otherTimesExplored == timesExplored && resends.size() > otherResends.size()))
                {
                    timesExplored = otherTimesExplored;
                    resends = std::move(otherResends);
                }
            }

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

        auto initializePath = [&]<typename ObservationType>(
                const Limits &limits,
                auto &createArrivalData,
                std::unordered_map<PositionAndVelocity, Path<ObservationType>> &rootNodes
            ) -> std::pair<Path<ObservationType>*, PathNode<ObservationType>*>
        {
            startOfExplorationWindowFrame = INT_MAX;
            noResendPath.clear();

            // Simulate the path with no resends
            auto simulatedPath = worker->simulateGatherPath({});
            if (!simulatedPath.has_value())
            {
                Log::Get() << "ERROR: Path could not be simulated"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                return {nullptr, nullptr};
            }

            // Get or create the root node
            auto currentPositionAndVelocity = PositionAndVelocity(worker);
            auto rootNodeIt = rootNodes.find(currentPositionAndVelocity);
            if (rootNodeIt == rootNodes.end())
            {
                rootNodeIt = rootNodes.emplace(currentPositionAndVelocity, Path<ObservationType>{currentPositionAndVelocity}).first;
            }
            auto &rootNode = rootNodeIt->second;

            // Create the arrival data
            ObservationType arrivalData = createArrivalData(*simulatedPath);

            // Step through the path, referencing or creating new nodes as needed, and making the no-resend observations
            auto &positions = std::get<0>(*simulatedPath);
            auto previousPosition = worker->getExactPosition();
            auto nextPositions = &rootNode.nextPositions;
            PathNode<ObservationType> *startOfExplorationWindow = nullptr;
            for (auto positionIt = positions.begin(); positionIt != positions.end(); positionIt++)
            {
                auto &position = *positionIt;
                auto node = getNextPathNode(*nextPositions, position - previousPosition, true);

                // The arrival delay is the distance to the last position node, which is the arrival position
                auto arrivalDelay = std::distance(positionIt, positions.end());

                // If the node hasn't been initialized yet, set if it is before or after the exploration window
                // Node types for nodes within the observation window will be initialized when we do the resend exploration and planning
                if (node->type == NodeType::Uninitialized)
                {
                    if (arrivalDelay > limits.startOfExplorationWindow)
                    {
                        node->type = NodeType::BeforeExplorationWindow;
                    }
                    else if (arrivalDelay < limits.endOfExplorationWindow)
                    {
                        node->type = NodeType::AfterExplorationWindow;
                    }
                }

                // Make the observation on the node
                if (node->type != NodeType::BeforeExplorationWindow && node->type != NodeType::AfterExplorationWindow)
                {
                    arrivalData.setArrivalDelay(arrivalDelay);
                    addArrivalObservation(node->arrivalData, arrivalData);
                }

                // When we reach the start of the exploration window, set some state
                if (arrivalDelay == limits.startOfExplorationWindow)
                {
                    startOfExplorationWindow = node;
                    startOfExplorationWindowFrame = currentFrame + (int)std::distance(positions.begin(), positionIt) + 1;
                    noResendPath = std::vector<BWAPI::ExactPosition>(positionIt, positions.end());
                    noResendStartOfNextPath = PositionAndVelocity(std::get<1>(*simulatedPath));
                }

                nextPositions = &node->nextPositions;
                previousPosition = position;
            }

            return {&rootNode, startOfExplorationWindow};
        };

        // Explores the given path, simulating resends where appropriate
        // Returns the least-explored resend plan with its exploration count
        auto explorePath = [&]<typename ObservationType, typename NextObservationType>(
                const Limits &limits,
                auto &createArrivalData,
                std::unordered_map<PositionAndVelocity, Path<NextObservationType>> &nextPathRootNodes,
                PathNode<ObservationType> *startOfExplorationWindow)
        {
            auto self = [&](                                                                                                // NOLINT(*-no-recursion)
                    auto &self,
                    std::vector<BWAPI::ExactPosition> &path,
                    PositionAndVelocity nextPathStartPos,
                    PathNode<ObservationType> *startNode,
                    int startFrame,
                    std::set<int> &previousResendFrames) -> LeastExploredNode
            {
                // Initialize the best result to the case where we follow the path to its conclusion
                LeastExploredNode bestResult{getTimesExplored(nextPathRootNodes, nextPathStartPos), previousResendFrames};

                // Loop through the nodes
                auto positionIt = path.begin();
                auto node = startNode;
                auto frame = startFrame;
                while (node && node->type != NodeType::AfterExplorationWindow)
                {
                    // Initialize the nodes where we can't resend at this frame because of Unit_Busy
                    if (node->type == NodeType::Uninitialized && previousResendFrames.contains(frame - BWAPI::Broodwar->getFrameCount()))
                    {
                        node->type = NodeType::StableNode;
                    }

                    // If the node is one where resends may be applicable, simulate the path with a resend from here
                    if (node->type == NodeType::Uninitialized || node->type == NodeType::NonfinalResendNode
                        || node->type == NodeType::FinalResendNode)
                    {
                        std::set<int> resendFrames = previousResendFrames;
                        resendFrames.insert(frame);
                        auto simulatedPath = worker->simulateGatherPath(resendFrames);
                        if (!simulatedPath.has_value())
                        {
                            Log::Get() << "ERROR: Resend path could not be simulated"
                                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                            break;
                        }
                        auto &resendPath = std::get<0>(*simulatedPath);

                        // If this is the first time simulating a resend from this node, set whether the path changes or not
                        if (node->type == NodeType::Uninitialized)
                        {
                            auto firstIt = positionIt + 1;
                            auto secondIt = resendPath.begin();
                            bool pathsEqual = true;
                            while (pathsEqual && firstIt != path.end() && secondIt != resendPath.end())
                            {
                                pathsEqual = (*firstIt == *secondIt);
                                firstIt++;
                                secondIt++;
                            }
                            pathsEqual = pathsEqual && firstIt == path.end() && secondIt == resendPath.end();

                            if (pathsEqual)
                            {
                                node->type = NodeType::StableNode;
                            }
                            else if (resendFrames.size() == limits.resends)
                            {
                                node->type = NodeType::FinalResendNode;
                            }
                            else
                            {
                                node->type = NodeType::NonfinalResendNode;
                            }
                        }

                        // Apply the results if the node is a resend node
                        if (node->type == NodeType::NonfinalResendNode || node->type == NodeType::FinalResendNode)
                        {
                            // Create the arrival data and apply it to the current node
                            ObservationType arrivalData = createArrivalData(*simulatedPath);
                            addArrivalObservation(node->arrivalDataAfterResend, arrivalData);

                            // If this is a non-final resend node, create or update all of the nodes along the resend path
                            if (node->type == NodeType::NonfinalResendNode)
                            {
                                auto &resendPositions = std::get<0>(*simulatedPath);
                                auto previousPosition = *positionIt;
                                auto nextPositions = &node->nextPositionsAfterResend;
                                PathNode<ObservationType> *firstNodeOnPath = nullptr;
                                for (auto resendPositionIt = resendPositions.begin();
                                    resendPositionIt != resendPositions.end();
                                    resendPositionIt++)
                                {
                                    auto &position = *resendPositionIt;
                                    auto resendNode = getNextPathNode(*nextPositions, position - previousPosition, true);
                                    if (!firstNodeOnPath) firstNodeOnPath = resendNode;

                                    // The arrival delay is the distance to the last position node, which is the arrival position
                                    auto arrivalDelay = std::distance(resendPositionIt, resendPositions.end());

                                    // If the node hasn't been initialized yet, set if it is before or after the exploration window
                                    // Node types for nodes within the observation window will be initialized when we do the resend exploration and
                                    // planning
                                    if (resendNode->type == NodeType::Uninitialized)
                                    {
                                        if (arrivalDelay > limits.startOfExplorationWindow)
                                        {
                                            resendNode->type = NodeType::BeforeExplorationWindow;
                                        }
                                        else if (arrivalDelay < limits.endOfExplorationWindow)
                                        {
                                            resendNode->type = NodeType::AfterExplorationWindow;
                                        }
                                    }

                                    // Make the observation on the node
                                    if (resendNode->type != NodeType::BeforeExplorationWindow && resendNode->type != NodeType::AfterExplorationWindow)
                                    {
                                        arrivalData.setArrivalDelay(arrivalDelay);
                                        addArrivalObservation(resendNode->arrivalData, arrivalData);
                                    }

                                    nextPositions = &resendNode->nextPositions;
                                    previousPosition = position;
                                }

                                // Now explore further down the next layer
                                auto result = self(self,
                                                   resendPositions,
                                                   PositionAndVelocity(std::get<1>(*simulatedPath)),
                                                   firstNodeOnPath,
                                                   frame + 1,
                                                   resendFrames);
                                bestResult.updateIfBetter(result);
                            }

                            // Update the best result
                            bestResult.updateIfBetter(nextPathRootNodes, arrivalData.nextPathStartPosition, std::move(resendFrames));
                        }
                    }

                    // Move to the next node along the no-resend path
                    positionIt++;
                    if (positionIt == path.end()) break;
                    node = getNextPathNode(node->nextPositions, *positionIt - *(positionIt - 1), false);
                    frame++;
                }

                return bestResult;
            };

            std::set<int> previousResends;
            return std::move(self(self, noResendPath, noResendStartOfNextPath, startOfExplorationWindow, currentFrame, previousResends).resends);
        };

        auto planAndExecuteResends = [&]<typename ObservationType, typename NextObservationType>(
                const Limits &limits,
                auto &createArrivalData,
                std::unordered_map<PositionAndVelocity, Path<NextObservationType>> &nextPathRootNodes,
                std::pair<Path<ObservationType>*, PathNode<ObservationType>*> &currentPath)
        {
            if (!currentPath.first || !currentPath.second) return;

            // Explore resends and plan our desired path when we reach the start of the exploration window
            if (currentFrame == startOfExplorationWindowFrame)
            {
                plannedResendFrames = std::move(explorePath(limits, createArrivalData, nextPathRootNodes, currentPath.second));
                currentPath.first->timesExplored++;
            }

            // Execute a desired resend
            if (plannedResendFrames.contains(currentFrame))
            {
                bool result;
                if constexpr(std::is_same_v<ObservationType, GatherArrivalData>)
                {
                    result = worker->gather(patch);
                }
                else
                {
                    result = worker->rightClick(depot);
                }
                if (!result)
                {
                    Log::Get() << "ERROR: Failed to reissue gather or return command: " << BWAPI::Broodwar->getLastError()
                               << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                }
            }
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

                // Plan and execute resends as appropriate
                planAndExecuteResends(
                        {GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END, GATHER_RESEND_LIMIT},
                        createGatherArrivalData,
                        mapData.resourceToReturnPaths[TilePosition::fromBWAPI(patch->getTilePosition())],
                        currentGatherPath);

                return;
            }
            case 1:
            {
                // Worker is mining; transition to state 2 when it is finished mining
                if (worker->getOrder() == BWAPI::Orders::ReturnMinerals && worker->isCarryingMinerals())
                {
                    CherryVis::log(worker->getID()) << "State transition from mining to returning minerals";
                    state = 2;
                    currentReturnPath = initializePath(
                            {RETURN_EXPLORATION_WINDOW_START, RETURN_EXPLORATION_WINDOW_END},
                            createReturnArrivalData,
                            mapData.resourceToReturnPaths[TilePosition::fromBWAPI(patch->getTilePosition())]);
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
                    currentGatherPath = initializePath(
                            {GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END},
                            createGatherArrivalData,
                            mapData.resourceToGatherPaths[TilePosition::fromBWAPI(patch->getTilePosition())]);
                }

                // Plan and execute resends as appropriate
                planAndExecuteResends(
                        {RETURN_EXPLORATION_WINDOW_START, RETURN_EXPLORATION_WINDOW_END, RETURN_RESEND_LIMIT},
                        createReturnArrivalData,
                        mapData.resourceToGatherPaths[TilePosition::fromBWAPI(patch->getTilePosition())],
                        currentReturnPath);

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
