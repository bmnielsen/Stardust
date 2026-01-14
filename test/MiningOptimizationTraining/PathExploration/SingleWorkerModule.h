#pragma once

#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    // Module that just takes one of the initial workers and orders it to mine
    template <typename WorkerStatusType>
    class SingleWorkerModule : public PathExplorationModule<WorkerStatusType>
    {
    public:
        explicit SingleWorkerModule(BWAPI::TilePosition patchTile) : PathExplorationModule<WorkerStatusType>(), patchTile(patchTile) {}

    protected:
        using PathExplorationModule<WorkerStatusType>::workerStatuses;
        using PathExplorationModule<WorkerStatusType>::mapData;

        bool initialize() override;

    private:
        BWAPI::TilePosition patchTile;
    };
}
