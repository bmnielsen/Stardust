#pragma once

#include "PathExplorationModule.h"
#include "WorkerPathExploration.h"

namespace MiningOptimizationTraining
{
    // Module that does path exploration on every patch on the map
    class FullSaturationModule : public PathExplorationModule
    {
    public:
        explicit FullSaturationModule(unsigned int cannons)
                : PathExplorationModule()
                , cannons(cannons)
        {}

    protected:
        bool initialize() override;

    private:
        unsigned int cannons;

        std::map<BWAPI::Position, std::pair<int, Base *>> workerCreationOrderAndBase;
    };
}
