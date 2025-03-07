#include "BWTest.h"

#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"
#include "StardustAIModule.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/Macro/SaturateBases.h"
#include "WorkerMiningInstrumentation.h"
#include "MiningOptimization/WorkerMiningOptimization.h"
#include "Units.h"
#include "Workers.h"
#include "BuildingPlacement.h"
#include "Geo.h"

#include <algorithm>
#include <random>

// This file is used to train the initial worker split
// As the timings vary slightly based on the random heading of the initial workers, we run lots of tests of mining from each patch
// from each start position and keep track of the average results.
// The order process timer reset of the workers is affected by the enemy start location, so we run the tests with each combination
// of player start locations.
// Spawn position training must be done prior to running this training.
namespace
{
    struct InitialWorkerState
    {
        MyWorker worker;
        int workerIndex;
        Resource resource;

        int completedReturns = 0;
    };

    void runTest(BWTest &test)
    {
        test.opponentRace = BWAPI::Races::Random;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.myModule = []()
        {
            return new StardustAIModule();
        };
        test.expectWin = false;

        test.onStartMine = []()
        {
            Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

            // Add a dummy main army play since one is needed
            std::vector<std::shared_ptr<Play>> openingPlays;
            openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(Map::getMyMain()));
            Strategist::setOpening(openingPlays);
        };

        std::list<InitialWorkerState> workerStates;
        test.onFrameMine = [&workerStates]()
        {
            if (BWAPI::Broodwar->getFrameCount() == 0)
            {
                std::set<Resource> availableResources(Map::getMyMain()->mineralPatches().begin(), Map::getMyMain()->mineralPatches().end());
                for (auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
                {
                    auto worker = std::static_pointer_cast<MyWorkerImpl>(unit);

                    // Find the index of the worker
                    int idx = 0;
                    for (auto &startingWorkerPosition :
                        Map::mapSpecificOverride()->startingWorkerPositions(BWAPI::Broodwar->self()->getStartLocation()))
                    {
                        if (worker->lastPosition == startingWorkerPosition) break;
                        idx++;
                    }
                    if (idx == 4)
                    {
                        std::cout << "ERROR: Could not find worker " << worker->lastPosition << " in start worker positions" << std::endl;
                        BWAPI::Broodwar->leaveGame();
                        return;
                    }

                    // Find the patch with the least observations and assign the worker to it
                    uint32_t leastObservations = UINT32_MAX;
                    Resource best = nullptr;
                    for (auto &resource : availableResources)
                    {
                        auto observationCount =
                                WorkerMiningOptimization::resourceObservationsFor(resource).startingWorkerObservationsFor(idx).observationCount;
                        if (observationCount < leastObservations)
                        {
                            leastObservations = observationCount;
                            best = resource;
                        }
                    }
                    if (!best)
                    {
                        std::cout << "ERROR: No resources available for worker " << worker->lastPosition << std::endl;
                        BWAPI::Broodwar->leaveGame();
                        return;
                    }

                    Workers::setWorkerMineralPatch(worker, best, Map::getMyMain());
                    workerStates.emplace_back(worker, idx, best);
                    availableResources.erase(best);
                }
            }

            bool anyRemaining = false;
            for (auto &workerState : workerStates)
            {
                if (workerState.worker->lastCarryingResourceChange == currentFrame && !workerState.worker->carryingResource)
                {
                    workerState.completedReturns++;
                    if (workerState.completedReturns == 2)
                    {
                        WorkerMiningOptimization::resourceObservationsFor(workerState.resource)
                            .startingWorkerObservationsFor(workerState.workerIndex)
                            .addObservation(currentFrame);
                    }
                }
                if (workerState.completedReturns < 2) anyRemaining = true;
            }
            if (!anyRemaining)
            {
                BWAPI::Broodwar->leaveGame();
                workerStates.clear();
            }
        };

        test.run();
    }
}

TEST(InitialSplitTraining, Vermeer)
{
    WorkerMiningOptimization::setExploring(false);
    WorkerMiningOptimization::setUpdateResourceObservations(true);

    Maps::RunOnEachStartLocationPairAndRandomRace(Maps::Get("aiide2024/(4)Vermeer"), [](BWTest test)
    {
        runTest(test);
    });
}

