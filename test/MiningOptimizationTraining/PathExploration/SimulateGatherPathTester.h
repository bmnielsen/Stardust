#pragma once

#include "BWAPI.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    class SimulateGatherPathTester
    {
    public:
        SimulateGatherPathTester(MapData &mapData, BWAPI::Unit worker, BWAPI::Unit patch, BWAPI::Unit depot)
            : worker(worker)
            , patch(patch)
            , depot(depot)
            , state(0)
            , preMiningFrames(0)
            , followingPath(false)
        {}

        void update();

        void outputDebugInformation() {}

    private:
        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        // State for the state machine. Possible values:
        // 0 - approaching the patch
        // 1 - pre-mining
        // 2 - mining
        // 3 - approaching the depot
        int state;

        // How many frames the worker has been in the pre-mining state
        unsigned int preMiningFrames;

        // Whether there is a path being followed
        bool followingPath;

        // Frame numbers where resends are planned
        std::set<int> plannedResendFrames;

        // The path we expect to observe, which we trim from the front on each frame
        std::deque<BWAPI::ExactPosition> expectedPath;

        // The expected arrival data (excluding delay) of the previously-planned gather path
        std::unique_ptr<GatherArrivalData> expectedGatherArrivalData;

        // The expected arrival data (excluding delay) of the previously-planned return path
        std::unique_ptr<ReturnArrivalData> expectedReturnArrivalData;
    };
}
