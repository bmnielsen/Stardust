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

        [[nodiscard]] bool isFinished() const { return state == 4; };

        void outputDebugInformation() const;

    private:
        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        std::unordered_map<PositionAndVelocity, GatherPath> &gatherPaths;
        std::unordered_map<PositionAndVelocity, ReturnPath> &returnPaths;

        // State for the state machine. Possible values:
        // 0 - approaching the patch
        // 1 - wait for minerals
        // 2 - mining
        // 3 - approaching the depot
        // 4 - finished
        int state;

        // Count of how many times the worker has transitioned out of each state
        std::map<int, unsigned int> stateCount;

        // The planned resend frames
        std::set<int> plannedResendFrames;

        // Planned frame on which to set the order process timer to a specific value
        std::pair<int, int> plannedSetOrderProcessTimerFrame;
    };
}
