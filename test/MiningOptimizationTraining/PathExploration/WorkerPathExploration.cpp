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
                    // TODO

                    break;
                }
                case 1:
                {
                    // Nothing is needed in this state
                    break;
                }
                case 2:
                {
                    // TODO

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
                    case -1:
                    {
                        // Assumption is that the worker has just been ordered to gather
                        stateTransition(0);
                        break;
                    }
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

        previousPosition = worker->getExactPosition();
    }
}
