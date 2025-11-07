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
            , startOfGatherExplorationWindow(nullptr)
            , startOfReturnExplorationWindow(nullptr)
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

        // Pointer to the gather path node that starts the gather exploration window
        GatherPathNode *startOfGatherExplorationWindow;

        // Pointer to the return path node that starts the return exploration window
        ReturnPathNode *startOfReturnExplorationWindow;

        // The frame where we will reach the start of the exploration window
        int startOfExplorationWindowFrame;

        // The planned resend frames
        std::set<int> plannedResendFrames;
    };
}
