#pragma once

#include <BWAPI/StateCopy.h>

#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    // Module that does path exploration on every patch on the map
    template <typename WorkerStatusType>
    class FullSaturationModule : public PathExplorationModule<WorkerStatusType>
    {
    public:
        explicit FullSaturationModule(unsigned int cannons, bool oneBase = false)
                : PathExplorationModule<WorkerStatusType>()
                , cannons(cannons)
                , oneBase(oneBase)
                , utilityWorker(nullptr)
        {}

    protected:
        using PathExplorationModule<WorkerStatusType>::workerStatuses;
        using PathExplorationModule<WorkerStatusType>::mapData;

        bool initialize() override;

    private:
        unsigned int cannons;
        bool oneBase;
        BWAPI::Unit utilityWorker;
        BWAPI::StateCopy initialState;

        std::map<BWAPI::Position, std::pair<int, Base *>> workerCreationOrderAndBase;
    };
}
