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
            , followingPath(false)
            , expectedNextPathStartPosition({})
        {}

        void update();

        void outputDebugInformation() {}

    private:
        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        // State for the state machine. Possible values:
        // 0 - approaching the patch
        // 1 - mining
        // 2 - approaching the depot
        int state;

        // Whether there is a path being followed
        bool followingPath;

        // Frame numbers where resends are planned
        std::set<int> plannedResendFrames;

        // The path we expect to observe, which we trim from the front on each frame
        std::deque<BWAPI::ExactPosition> expectedPath;

        // The expected start position of the next path
        BWAPI::ExactPosition expectedNextPathStartPosition;
    };
}
