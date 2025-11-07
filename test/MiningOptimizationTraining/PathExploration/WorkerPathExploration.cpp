#include "PathExplorationModule.h"

#include "Geo.h"

namespace MiningOptimizationTraining
{
    void WorkerPathExploration::update()
    {
        auto initializePath = [&]<typename ObservationType>(std::unordered_map<PositionAndVelocity, Path<ObservationType>> &rootNodes)
        {
            startOfExplorationWindowFrame = INT_MAX;

            // Simulate the path with no resends
            auto result = worker->simulateGatherPath({});
            if (!result.has_value())
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
//        auto &rootNode = *rootNodeIt;
//
//
//
//        auto currentPosition = worker->getExactPosition();
            return nullptr;
        };

        auto planAndExecuteResends = [&]<typename ObservationType>(PathNode<ObservationType> *startOfExplorationWindow)
        {

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
