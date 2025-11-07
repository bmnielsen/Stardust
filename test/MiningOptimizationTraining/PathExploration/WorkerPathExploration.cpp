#include "PathExplorationModule.h"

#include "../DataModel/Configuration.h"

namespace MiningOptimizationTraining
{
    namespace
    {
        template <typename ObservationType>
        PathNode<ObservationType> *getNextPathNode(std::vector<std::pair<PathNode<ObservationType>, uint32_t>> &nextPositions,
                                                   BWAPI::ExactPositionDifference positionDifference)
        {
            uint32_t totalOccurrences = 0;
            std::pair<PathNode<ObservationType>, uint32_t> *nextPathNodePair = nullptr;
            for (auto &pathNodePair : nextPositions)
            {
                if (pathNodePair.first.positionDifferenceFromPreviousNode == positionDifference) nextPathNodePair = &pathNodePair;
                totalOccurrences += pathNodePair.second;
            }

            if (!nextPathNodePair)
            {
                nextPathNodePair = &nextPositions.emplace_back(PathNode<ObservationType>{positionDifference}, 0);
            }

            if (totalOccurrences < UINT32_MAX)
            {
                nextPathNodePair->second++;
            }

            return &(nextPathNodePair->first);
        }

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
    }

    void WorkerPathExploration::update()
    {
        auto initializePath =
                [&]<typename ObservationType>(std::unordered_map<PositionAndVelocity, Path<ObservationType>> &rootNodes) -> PathNode<ObservationType>*
        {
            startOfExplorationWindowFrame = INT_MAX;

            // Simulate the path with no resends
            auto simulatedPath = worker->simulateGatherPath({});
            if (!simulatedPath.has_value())
            {
                Log::Get() << "ERROR: Path could not be simulated"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                return nullptr;
            }

            // Get or create the root node
            auto currentPositionAndVelocity = PositionAndVelocity(worker);
            auto rootNodeIt = rootNodes.find(currentPositionAndVelocity);
            if (rootNodeIt == rootNodes.end())
            {
                rootNodeIt = rootNodes.emplace(currentPositionAndVelocity, Path<ObservationType>{currentPositionAndVelocity}).first;
            }
            auto &rootNode = rootNodeIt->second;

            // Create the arrival data and set the bounds for the exploration window
            ObservationType arrivalData;
            size_t explorationWindowStart, explorationWindowEnd;
            if constexpr(std::is_same_v<ObservationType, GatherArrivalData>)
            {
                arrivalData = GatherArrivalData::createFromSimulatedPath(*simulatedPath, patch);
                explorationWindowStart = GATHER_EXPLORATION_WINDOW_START;
                explorationWindowEnd = GATHER_EXPLORATION_WINDOW_END;
            }
            else
            {
                arrivalData = ReturnArrivalData::createFromSimulatedPath(*simulatedPath);
                explorationWindowStart = RETURN_EXPLORATION_WINDOW_START;
                explorationWindowEnd = RETURN_EXPLORATION_WINDOW_END;
            }

            // Step through the path, referencing or creating new nodes as needed, and making the no-resend observations
            auto &positions = std::get<0>(*simulatedPath);
            auto previousPosition = worker->getExactPosition();
            auto nextPositions = &rootNode.nextPositions;
            PathNode<ObservationType> *startOfExplorationWindow = nullptr;
            for (auto positionIt = positions.begin(); positionIt != positions.end(); positionIt++)
            {
                auto &position = *positionIt;
                auto node = getNextPathNode(*nextPositions, position - previousPosition);

                // The arrival delay is the distance to the last position node, which is the arrival position
                auto arrivalDelay = std::distance(positionIt, positions.end());

                // Make the observation on the node
                arrivalData.setArrivalDelay(arrivalDelay);
                addArrivalObservation(node->arrivalData, arrivalData);

                // If the node hasn't been initialized yet, set if it is before or after the exploration window
                // Node types for nodes within the observation window will be initialized when we do the resend exploration and planning
                if (node->type == NodeType::Uninitialized)
                {
                    if (arrivalDelay > explorationWindowStart)
                    {
                        node->type = NodeType::BeforeExplorationWindow;
                    }
                    else if (arrivalDelay < explorationWindowEnd)
                    {
                        node->type = NodeType::AfterExplorationWindow;
                    }
                }

                // Store the pointer to the node that is our start point for exploration
                if (arrivalDelay == explorationWindowStart)
                {
                    startOfExplorationWindow = node;
                    startOfExplorationWindowFrame = currentFrame + (int)std::distance(positions.begin(), positionIt) + 1;
                }

                nextPositions = &node->nextPositions;
                previousPosition = position;
            }

            return startOfExplorationWindow;
        };

        auto planAndExecuteResends = [&]<typename ObservationType>(PathNode<ObservationType> *startOfExplorationWindow)
        {
            // Explore resends and plan our desired path when we reach the start of the exploration window
            if (currentFrame == startOfExplorationWindowFrame)
            {
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
                planAndExecuteResends(startOfGatherExplorationWindow);

                return;
            }
            case 1:
            {
                // Worker is mining; transition to state 2 when it is finished mining
                if (worker->getOrder() == BWAPI::Orders::ReturnMinerals && worker->isCarryingMinerals())
                {
                    CherryVis::log(worker->getID()) << "State transition from mining to returning minerals";
                    state = 2;
                    startOfReturnExplorationWindow = initializePath(mapData.resourceToReturnPaths[TilePosition::fromBWAPI(patch->getTilePosition())]);
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
                    startOfGatherExplorationWindow = initializePath(mapData.resourceToGatherPaths[TilePosition::fromBWAPI(patch->getTilePosition())]);
                }

                // Plan and execute resends as appropriate
                planAndExecuteResends(startOfReturnExplorationWindow);

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
