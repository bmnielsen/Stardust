#pragma once

#include "BWAPI.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

#define VALIDATE_EXPECTED_TRANSITION_FRAMES true

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
            , expectedTransitionFrame(-1)
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
        // 1 - wait for minerals
        // 2 - mining
        // 3 - approaching the depot
        int state;

        // Count of how many times the worker has transitioned out of each state
        std::map<int, unsigned int> stateCount;

        // The planned resend frames
        std::set<int> plannedResendFrames;

#if VALIDATE_EXPECTED_TRANSITION_FRAMES
        // The next expected transition frame (from approaching patch to WaitForMinerals, or from approaching depot to delivering minerals)
        int expectedTransitionFrame;
#endif
    };
}
