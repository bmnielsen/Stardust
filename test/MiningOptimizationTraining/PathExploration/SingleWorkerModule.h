#pragma once

#include "PathExplorationModule.h"
#include "WorkerPathExploration.h"

namespace MiningOptimizationTraining
{
    // Module that just takes one of the initial workers and orders it to mine
    class SingleWorkerModule : public PathExplorationModule
    {
    protected:
        bool initialize() override;
    };
}
