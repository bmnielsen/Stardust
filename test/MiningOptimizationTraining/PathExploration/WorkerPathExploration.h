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
            , previousPosition(worker->getExactPosition())
            , state(-1)
        {}

        void update();

    private:
        MapData &mapData;

        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        // Stores the previous exact position of the worker (including subpixels)
        BWAPI::ExactPosition previousPosition;

        /* State specific to the gather phase */



        /* State specific to the return phase */


        /* State used by both phases */

        // State for the state machine. Possible values:
        // -1 - uninitialized
        // 0 - approaching the patch
        // 1 - mining
        // 2 - approaching the depot
        int state;

        // Called for workers that are on their way to gather minerals
        void gathering();

        // Called for workers that are on their way to return minerals
        void returning();
    };
}
