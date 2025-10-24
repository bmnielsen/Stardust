#include "PathExplorationModule.h"

#include "Geo.h"

namespace MiningOptimizationTraining
{
    void WorkerPathExploration::update()
    {
        auto stateTransition = [&](int newState)
        {
            // Update the state
            CherryVis::log(worker->getID()) << "State transition from " << state << " to " << newState;
            state = newState;

            // Reset data relating to the new state
            switch (newState)
            {
                case 0:
                {
                    gatherPositionHistory.clear();

                    break;
                }
                case 1:
                {
                    // Nothing is needed in this state
                    break;
                }
                case 2:
                {
                    returnPositionHistory.clear();

                    break;
                }
                default:
                {
                    Log::Get() << "ERROR: Worker transitioning to unknown state " << newState;
                    break;
                }
            }
        };

        // Run the state machine
        [&]()
        {
            while (true)
            {
                switch (state)
                {
                    case 0:
                    {
                        // Worker is approaching the patch; transition to state 1 when it starts mining
                        if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
                        {
                            stateTransition(1);
                            break;
                        }

                        gathering();

                        return;
                    }
                    case 1:
                    {
                        // Worker is mining; transition to state 2 when it is finished mining
                        if (worker->getOrder() != BWAPI::Orders::MiningMinerals && worker->isCarryingMinerals())
                        {
                            stateTransition(2);
                            break;
                        }

                        // Nothing is needed in this state

                        return;
                    }
                    case 2:
                    {
                        // Worker is returning minerals; transition to state 0 when it has returned minerals
                        if (!worker->isCarryingMinerals())
                        {
                            // Run the returning handler once more to record the data before switching states
                            returning();

                            stateTransition(0);
                            break;
                        }

                        returning();

                        return;
                    }
                    default:
                    {
                        Log::Get() << "ERROR: Worker has unknown state " << state;
                        return;
                    }
                }
            }
        }();

        previousPosition = SubpixelPosition(worker);
    }

    ParsedPositionHistory WorkerPathExploration::parsePositionHistory(
            std::vector<std::shared_ptr<const PositionOnPath>> &positionHistory,
            BWAPI::Unit start,
            BWAPI::Unit target)
    {
        // Result defaults to invalid
        ParsedPositionHistory result(positionHistory);

        // Validate the path exists
        if (positionHistory.empty())
        {
            Log::Get() << "ERROR: Empty path"
                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
            return result;
        }

        // Ignore if the path didn't start at the correct location
        // TODO: Will need to loosen this when we implement spawn position training
        if (Geo::EdgeToEdgeDistance(worker->getType(),
                                    (*positionHistory.begin())->bwapiPosition(),
                                    start->getType(),
                                    start->getPosition()) > 0)
        {
            CherryVis::log(worker->getID()) << "Ignoring path that did not start at " << start->getType();
            return result;
        }

        // Extract the arrival and resend positions
        // TODO: Extract resend positions
        auto finalWorkerPosition = (*positionHistory.rbegin())->bwapiPosition();
        for (auto it = positionHistory.begin(); it != positionHistory.end(); it++)
        {
            auto dist = Geo::EdgeToEdgeDistance(worker->getType(), (*it)->bwapiPosition(), target->getType(), target->getPosition());

            // Arrival position is defined as the position where:
            // - distance to the target is 0
            // - position is the same as the worker's current position and remains stable for the remainder of the path
            // TODO: Check if we need to use the stable heading logic that we had in the original implementation
            //       My hypothesis is that this was only needed to cover patch switch cases that will no longer be relevant
            if (result.arrivalPositionIt == positionHistory.end() && dist == 0)
            {
                bool stablePosition = true;
                for (auto stableIt = it; stableIt != positionHistory.end(); stableIt++)
                {
                    if (finalWorkerPosition != (*stableIt)->bwapiPosition())
                    {
                        stablePosition = false;
                        break;
                    }
                }
                if (stablePosition)
                {
                    result.arrivalPositionIt = it;
                }
            }
        }

        // If the arrival position was not found, jump out
        if (result.arrivalPositionIt == positionHistory.end())
        {
            Log::Get() << "ERROR: Arrival position not found"
                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
            return result;
        }

        // TODO: Validate that all resend positions are found

        // For mining, determine if the worker's heading will require turning to start mining
        // The worker has two frames to turn before the order process timer will be nonzero and it will have to wait 9 frames
        // The worker always points directly at the center of the patch when mining
        if (target->getType().isMineralField())
        {
            auto vectorToPatch = target->getPosition() - finalWorkerPosition;
            auto angleDiff = Geo::BWAngleDiff(Geo::BWHeading(worker->getAngle()), Geo::BWDirection(vectorToPatch));
            if (angleDiff > 2 * worker->getType().turnRadius())
            {
                result.facingTarget = false;
                CherryVis::log(worker->getID()) << "Not facing patch, angle diff is " << angleDiff;
            }
        }

        result.valid = true;
        return result;
    }
}
