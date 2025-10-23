#pragma once

#include "BWAPI.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    class WorkerPathExploration
    {
    public:
        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        WorkerPathExploration(BWAPI::Unit worker, BWAPI::Unit patch, BWAPI::Unit depot)
            : worker(worker)
            , patch(patch)
            , depot(depot)
        {}

        void update(MapData &mapData);
    };
}
