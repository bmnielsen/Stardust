#pragma once

#include <BWAPI.h>

#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    struct GatheringWorkersModuleOptions : PathExplorationModuleOptions
    {
        BWAPI::TilePosition oneBase = BWAPI::TilePositions::Invalid;
        BWAPI::TilePosition onePatch = BWAPI::TilePositions::Invalid;
    };

    // Module that does path exploration on every patch on the map
    template <typename WorkerStatusType>
    class GatheringWorkersModule : public PathExplorationModule
    {
    public:
        explicit GatheringWorkersModule(const GatheringWorkersModuleOptions &options)
                : PathExplorationModule(options)
                , options(options)
        {}

    protected:

        bool initialize() override;
        void run() override;

    private:
        const GatheringWorkersModuleOptions &options;
        std::map<BWAPI::Position, std::pair<int, Base *>> workerCreationOrderAndBase;
        std::vector<std::unique_ptr<WorkerStatusType>> workerStatuses;
    };
}
