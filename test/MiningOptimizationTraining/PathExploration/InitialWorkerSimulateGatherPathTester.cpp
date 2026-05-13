#include "InitialWorkerSimulateGatherPathTester.h"

#include "Units.h"
#include "Map.h"

#include <BWAPI/SimulateGatherPathOptions.h>

#include <random>
#include <gtest/gtest.h>

#define VERBOSE_LOGGING false

namespace MiningOptimizationTraining
{
    namespace
    {
        std::default_random_engine rng;

        std::set<int> planResendFrames(bool allowTwoResends, int startFrame, int endFrame)
        {
            std::vector<std::set<int>> resendCombinations;
            resendCombinations.emplace_back(); // the no resend option
            for (int firstResend = startFrame; firstResend <= endFrame; firstResend++)
            {
                resendCombinations.push_back({firstResend});
                if (allowTwoResends)
                {
                    for (int secondResend = (firstResend + 1); secondResend <= endFrame; secondResend++)
                    {
                        if (secondResend == (firstResend + BWAPI::Broodwar->getLatencyFrames())) continue;
                        resendCombinations.push_back({firstResend});
                    }
                }
            }

            std::uniform_int_distribution<size_t> dist(0, resendCombinations.size() - 1);
            return resendCombinations[dist(rng)];
        }
    }

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

        // On startup, copy the state and create the state objects for each worker
        if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            initialState = BWAPI::Broodwar->getStateCopy();

            // Pick a random set of patches for each worker
            auto firstPatches = Map::getMyMain()->mineralPatches();
            auto secondPatches = Map::getMyMain()->mineralPatches();
            rng = std::default_random_engine(BWAPI::Broodwar->getRandomSeed());
            std::shuffle(std::begin(firstPatches), std::end(firstPatches), rng);
            std::shuffle(std::begin(secondPatches), std::end(secondPatches), rng);
            size_t i = 0;
            for (auto worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (!worker->getType().isWorker()) continue;

#if VERBOSE_LOGGING
                Log::Get() << worker->getID() << ": Picked patch @ " << firstPatches[i]->tile << " and @ " << secondPatches[i]->tile;
#endif

                workers.emplace_back(initialState, worker, firstPatches[i]->getBwapiUnitIfVisible(), secondPatches[i]->getBwapiUnitIfVisible());
                i++;
            }
        }

        if (BWAPI::Broodwar->getFrameCount() >= 5)
        {
            for (auto &worker : workers)
            {
                worker.onFrame();
            }
        }

        InstrumentedDoNothingModule::onFrameEnd();
    }

    void InitialWorkerSimulateGatherPathTesterModule::onEnd(bool isWinner)
    {
        InstrumentedDoNothingModule::onEnd(isWinner);
    }

    void InitialWorkerSimulateGatherPathTesterModule::WorkerState::onFrame()
    {
        positionHistory.emplace_back(worker->getExactPosition());

#if VERBOSE_LOGGING
        Log::Get() << worker->getID() << ": " << worker->getExactPosition();
#endif

        // Validates that the position history matches the simulated positions
        auto validatePositionHistory = [&]()
        {
            auto historyIt = positionHistory.begin();
            auto simulatedIt = simulatedGatherPath->positions.begin();
            while (historyIt != positionHistory.end() && simulatedIt != simulatedGatherPath->positions.end())
            {
                if (*historyIt != *simulatedIt)
                {
                    Log::Get() << "ERROR: " << worker->getID() << ": Actual positions do not match simulated positions: "
                               << *historyIt
                               << " vs. "
                               << *simulatedIt;
                    return false;
                }

                historyIt++;
                simulatedIt++;
            }

            // The actual history is allowed to be longer than the simulated history, but not vice versa
            if (simulatedIt != simulatedGatherPath->positions.end())
            {
                Log::Get() << "ERROR: " << worker->getID() << ": More positions in simulated than actual";
                return false;
            }

            return true;
        };

#if VERBOSE_LOGGING
        auto formatResendFrames = [](std::set<int> &resendFrames)
        {
            std::ostringstream buf;
            std::string sep;
            buf << "[";
            for (auto resendFrame : resendFrames)
            {
                buf << sep << resendFrame;
                sep = ",";
            }
            buf << "]";
            return buf.str();
        };
#endif

        switch (state)
        {
            case 0:
                {
                    // Initial state; order the worker to mine the first patch
                    worker->gather(firstPatch);
                    state = 1;

                    plannedResendFrames = planResendFrames(true, BWAPI::Broodwar->getFrameCount() + 8, BWAPI::Broodwar->getFrameCount() + 20);
#if VERBOSE_LOGGING
                    Log::Get() << worker->getID() << ": Resend frames " << formatResendFrames(plannedResendFrames);
#endif
                    simulatedGatherPath = worker->simulateGatherPath(
                            BWAPI::SimulateGatherPathOptions(plannedResendFrames, initialState.state)
                                .setReturnStateAtStartOfNextPath()
                                .switchToPatch(firstPatch->getBWIndex())
                                .setIncludeAllPositions());
                    if (!simulatedGatherPath)
                    {
                        Log::Get() << "ERROR: Unable to simulate gather path";
                        state = 7;
                    }

                    positionHistory.clear();
                }
                break;
            case 1:
            case 4:
                if (plannedResendFrames.contains(BWAPI::Broodwar->getFrameCount() + BWAPI::Broodwar->getLatencyFrames()))
                {
#if VERBOSE_LOGGING
                    Log::Get() << worker->getID() << ": Resending gather";
#endif
                    worker->gather((state == 1) ? firstPatch : secondPatch);
                }

                // Worker is moving towards the patch; wait until it starts mining
                if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    EXPECT_TRUE(validatePositionHistory());
                    state++;

#if VERBOSE_LOGGING
                    Log::Get() << worker->getID() << ": state " << state;
#endif
                }

                break;
            case 2:
            case 5:
                // Worker is mining the patch; wait until it is finished
                if (worker->getOrder() == BWAPI::Orders::ReturnMinerals)
                {
                    plannedResendFrames = planResendFrames(false, BWAPI::Broodwar->getFrameCount() + 8, BWAPI::Broodwar->getFrameCount() + 20);
#if VERBOSE_LOGGING
                    Log::Get() << worker->getID() << ": Resend frames " << formatResendFrames(plannedResendFrames);
#endif
                    simulatedGatherPath = worker->simulateGatherPath(
                            BWAPI::SimulateGatherPathOptions(plannedResendFrames, simulatedGatherPath->stateAtStartOfNextPath)
                                    .setReturnStateAtStartOfNextPath()
                                    .setIncludeAllPositions());
                    if (!simulatedGatherPath)
                    {
                        Log::Get() << "ERROR: Unable to simulate gather path";
                        state = 7;
                    }

                    positionHistory.clear();
                    state++;

#if VERBOSE_LOGGING
                    Log::Get() << worker->getID() << ": state " << state;
#endif
                }
                break;
            case 3:
            case 6:
                if (plannedResendFrames.contains(BWAPI::Broodwar->getFrameCount() + BWAPI::Broodwar->getLatencyFrames()))
                {
#if VERBOSE_LOGGING
                    Log::Get() << worker->getID() << ": Resending return cargo";
#endif
                    worker->returnCargo();
                }

                // Worker is returning minerals from the patch; wait until it has done so
                if (!worker->isCarryingMinerals())
                {
                    EXPECT_TRUE(validatePositionHistory());

                    if (state == 3)
                    {
                        plannedResendFrames = planResendFrames(true, BWAPI::Broodwar->getFrameCount() + 8, BWAPI::Broodwar->getFrameCount() + 20);
#if VERBOSE_LOGGING
                        Log::Get() << worker->getID() << ": Resend frames " << formatResendFrames(plannedResendFrames);
#endif

                        auto options =
                                BWAPI::SimulateGatherPathOptions(plannedResendFrames, simulatedGatherPath->stateAtStartOfNextPath)
                                        .setReturnStateAtStartOfNextPath()
                                        .setIncludeAllPositions();

                        if (firstPatch != secondPatch)
                        {
                            worker->gather(secondPatch);
                            options.switchToPatch(secondPatch->getBWIndex());
                        }

                        simulatedGatherPath = worker->simulateGatherPath(options);
                        if (!simulatedGatherPath)
                        {
                            Log::Get() << "ERROR: Unable to simulate gather path";
                            state = 7;
                        }

                        positionHistory.clear();
                    }

                    state++;

#if VERBOSE_LOGGING
                    Log::Get() << worker->getID() << ": state " << state;
#endif
                }
                break;
            case 7:
                // Final state
                worker->stop();
                return;
            default:
                Log::Get() << "ERROR: Unknown state " << state << " " << initialState.label;
                return;
        }
    }
}
