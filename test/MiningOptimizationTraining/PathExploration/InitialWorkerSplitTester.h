#pragma once

#include "Modules/InstrumentedDoNothingModule.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    struct PlannedPath
    {
        std::set<int> resends;
        std::set<int> actionFrames;

        friend std::ostream &operator<<(std::ostream &os, const PlannedPath &path)
        {
            auto outIntSet = [&os](const std::set<int> &intSet)
            {
                os << "[";
                std::string sep;
                for (auto val : intSet)
                {
                    os << sep << val;
                    sep = ",";
                }
                os << "]";
            };

            os << "resends: ";
            outIntSet(path.resends);
            os << "; actions: ";
            outIntSet(path.actionFrames);

            return os;
        }
    };
    struct WorkerGatherPlan
    {
        PlannedPath firstGather;
        PlannedPath firstReturn;
        PlannedPath secondGather;
        PlannedPath secondReturn;
    };

    struct WorkerStatus
    {
        WorkerGatherPlan gatherPlan;
        BWAPI::Unit firstPatch;
        BWAPI::Unit secondPatch;

        // 0 - initial state
        // 1 - moving towards first patch
        // 2 - mining first patch
        // 3 - moving towards first delivery
        // 4 - moving towards second patch
        // 5 - mining second patch
        // 6 - moving towards second delivery
        // 7 - final state
        int state = 0;
    };

    // Module to test that the trained initial worker split data is correct.
    // It takes the initial four workers and plans the best first two rotations. Along the way it verifies that all of the positions and timings
    // match the trained data.
    // The game will be initialized with the enemy as random since this is how we store the random seeds in our infrastructure. If the caller wishes
    // to test the behaviour as if the bot knew the enemy race, this can be specified in the constructor.
    // It can also be specified to pick random resends in order to test our path exploration.
    class InitialWorkerSplitTesterModule : public InstrumentedDoNothingModule
    {
    public:
        explicit InitialWorkerSplitTesterModule(const InitialWorkerMapData &mapData,
                                                BWAPI::Race enemyRace,
                                                bool chooseRandomResends)
            : mapData(mapData)
            , enemyRace(enemyRace)
            , chooseRandomResends(chooseRandomResends)
        {}

        void onStart() override;
        void onFrame() override;

    private:
        const InitialWorkerMapData &mapData;
        BWAPI::Race enemyRace;
        bool chooseRandomResends;

        std::vector<BWAPI::Unit> workers;
        std::vector<BWAPI::Unit> patches;

        std::map<BWAPI::Unit, WorkerStatus> workerStatuses;

        WorkerGatherPlan planPatchCombinationRandomly(BWAPI::Unit worker, BWAPI::Unit firstPatch, BWAPI::Unit secondPatch);
    };
}
