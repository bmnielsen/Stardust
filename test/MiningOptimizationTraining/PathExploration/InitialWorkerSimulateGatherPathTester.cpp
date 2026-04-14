#include "InitialWorkerSimulateGatherPathTester.h"

#include "Units.h"
#include "Map.h"

#include <random>

namespace MiningOptimizationTraining
{
    void InitialWorkerSimulateGatherPathTesterModule::onStart()
    {
        BWAPI::Broodwar->enableMiningTraining();

        InstrumentedDoNothingModule::onStart();

        // Initialize the minimum needed to have bases, start blocks and cannon placements available
        Units::initialize();
        Map::initialize();

        Log::Get() << "Initialized initial worker simulateGatherPath testing on "
                   << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";
    }

    void InitialWorkerSimulateGatherPathTesterModule::onFrame()
    {
        InstrumentedDoNothingModule::onFrameStart();

        // On the first frame, copy the state and create the state objects for each worker
        if (BWAPI::Broodwar->getFrameCount() == 0)
        {
            initialState = BWAPI::Broodwar->getStateCopy();

            // Pick a random set of patches for each worker
            auto firstPatches = Map::getMyMain()->mineralPatches();
            auto secondPatches = Map::getMyMain()->mineralPatches();
            auto rng = std::default_random_engine(BWAPI::Broodwar->getRandomSeed());
            std::shuffle(std::begin(firstPatches), std::end(firstPatches), rng);
            std::shuffle(std::begin(secondPatches), std::end(secondPatches), rng);
            size_t i = 0;
            for (auto worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (!worker->getType().isWorker()) continue;

                workers.emplace_back(initialState, worker, firstPatches[i]->getBwapiUnitIfVisible(), secondPatches[i]->getBwapiUnitIfVisible());
                i++;
            }
        }

        for (auto &worker : workers)
        {
            worker.onFrame();
        }

        InstrumentedDoNothingModule::onFrameEnd();
    }

    void InitialWorkerSimulateGatherPathTesterModule::onEnd(bool isWinner)
    {
        InstrumentedDoNothingModule::onEnd(isWinner);
    }

    void InitialWorkerSimulateGatherPathTesterModule::WorkerState::onFrame()
    {
        switch (state)
        {
            case 0:
                // Initial state; order the worker to mine the first patch
                worker->gather(firstPatch);
                state = 1;

                // TODO: Plan resends and record simulated path

                break;
            case 1:
            case 4:
                // TODO: Execute resends

                // Worker is moving towards the patch; wait until it starts mining
                if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    // TODO: Validate simulated gather path

                    state++;
                }

                break;
            case 2:
            case 5:
                // Worker is mining the patch; wait until it is finished
                if (worker->isCarryingMinerals())
                {
                    // TODO: Plan resends and record simulated path

                    state++;
                }
                break;
            case 3:
            case 6:
                // TODO: Add resend

                // Worker is returning minerals from the patch; wait until it has done so
                if (!worker->isCarryingMinerals())
                {
                    // TODO: Validate simulated return path

                    if (state == 3 && firstPatch != secondPatch)
                    {
                        worker->gather(secondPatch);
                    }
                    state++;
                }
                break;
            case 7:
                // Final state
                worker->stop();
                break;
            default:
                Log::Get() << "ERROR: Unknown state " << state << " " << initialState.label;
                break;
        }
    }
}
