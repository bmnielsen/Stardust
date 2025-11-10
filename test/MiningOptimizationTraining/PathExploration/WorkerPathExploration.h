#pragma once

#include "BWAPI.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    class WorkerPathExploration
    {
    public:
        WorkerPathExploration(MapData &mapData, BWAPI::Unit worker, BWAPI::Unit patch, BWAPI::Unit depot)
            : mapData(mapData)
            , worker(worker)
            , patch(patch)
            , depot(depot)
            , state(0)
            , currentGatherPath({nullptr, nullptr})
            , currentReturnPath({nullptr, nullptr})
            , startOfExplorationWindowFrame(INT_MAX)
        {}

        void update();

    private:
        MapData &mapData;

        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        // State for the state machine. Possible values:
        // 0 - approaching the patch
        // 1 - mining
        // 2 - approaching the depot
        int state;

        // Pointers to the current gather path (root and start of exploration window)
        std::pair<GatherPath*, GatherPathNode*> currentGatherPath;

        // Pointers to the current return path (root and start of exploration window)
        std::pair<ReturnPath*, ReturnPathNode*> currentReturnPath;

        // The frame where we will reach the start of the exploration window
        int startOfExplorationWindowFrame;

        // The no resend path, starting with the start of the exploration window
        std::vector<BWAPI::ExactPosition> noResendPath;

        // The start position of the next path after no resend
        PositionAndVelocity noResendStartOfNextPath;

        // The planned resend frames
        std::set<int> plannedResendFrames;
    };
}
