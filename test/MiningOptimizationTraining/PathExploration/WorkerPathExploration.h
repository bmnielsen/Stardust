#pragma once

#include "BWAPI.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    class WorkerPathExploration
    {
    public:
        WorkerPathExploration(MapData &mapData, BWAPI::Unit worker, BWAPI::Unit patch, BWAPI::Unit depot)
            : worker(worker)
            , patch(patch)
            , depot(depot)
            , gatherPaths(mapData.resourceToGatherPaths[TilePosition::fromBWAPI(patch->getTilePosition())])
            , returnPaths(mapData.resourceToReturnPaths[TilePosition::fromBWAPI(patch->getTilePosition())])
            , state(0)
        {}

        void update();

        void outputDebugInformation();

    private:
        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        std::unordered_map<PositionAndVelocity, GatherPath> &gatherPaths;
        std::unordered_map<PositionAndVelocity, ReturnPath> &returnPaths;

        // State for the state machine. Possible values:
        // 0 - approaching the patch
        // 1 - mining
        // 2 - approaching the depot
        int state;

        // The planned resend frames
        std::set<int> plannedResendFrames;
    };
}
