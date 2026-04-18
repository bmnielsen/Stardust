#pragma once

#include "Modules/InstrumentedDoNothingModule.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    struct WorkerGatherPlan
    {
        BWAPI::Unit firstPatch;
        std::set<int> firstGatherResends;
        std::set<int> expectedFirstGatherMiningFrames;
        std::set<int> firstReturnResends;
        std::set<int> expectedFirstReturnDeliveryFrames;

        BWAPI::Unit secondPatch;
        std::set<int> secondGatherResends;
        std::set<int> expectedSecondGatherMiningFrames;
        std::set<int> secondReturnResends;
        std::set<int> expectedSecondReturnDeliveryFrames;
    };

    // Module to test that the trained initial worker split data is correct.
    // It takes the initial four workers and plans the best first two rotations. Along the way it verifies that all of the positions and timings
    // match the trained data.
    // The game will be initialized with the enemy as random since this is how we store the random seeds in our infrastructure. If the caller wishes
    // to test the behaviour as if the bot knew the enemy race, this can be specified in the constructor.
    class InitialWorkerSplitTesterModule : public InstrumentedDoNothingModule
    {
    public:
        explicit InitialWorkerSplitTesterModule(const InitialWorkerMapData &mapData, BWAPI::Race knownEnemyRace = BWAPI::Races::Unknown)
            : mapData(mapData)
            , knownEnemyRace(knownEnemyRace)
        {}

        void onStart() override;
        void onFrame() override;

    private:
        const InitialWorkerMapData &mapData;
        BWAPI::Race knownEnemyRace;

        std::map<BWAPI::Unit, WorkerGatherPlan> workerGatherPlans;

        WorkerGatherPlan planPatchCombination(BWAPI::ExactPosition startPosition, TilePosition firstPatch, TilePosition secondPatch);
    };
}
